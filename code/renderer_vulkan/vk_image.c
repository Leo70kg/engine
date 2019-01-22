#include "tr_local.h"
#include "vk_image_sampler.h"
#include "R_ImageProcess.h"
#include "vk_image.h"
#include "vk_instance.h"
#include "tr_globals.h"
#include "tr_cvar.h"
//
// Memory allocations.
//
#define IMAGE_CHUNK_SIZE        (32 * 1024 * 1024)

struct Chunk {
	VkDeviceMemory memory;
	VkDeviceSize used;
};


struct Vk_Image {
	VkImage handle;

    // To use any VkImage, including those in the swap chain, int the render pipeline
    // we have to create a VkImageView object. An image view is quite literally a
    // view into image. It describe how to access the image and witch part of the
    // image to access, if it should be treated as a 2D texture depth texture without
    // any mipmapping levels.
    
    VkImageView view;

	// Descriptor set that contains single descriptor used to access the given image.
	// It is updated only once during image initialization.
	VkDescriptorSet descriptor_set;
};

// should be the same as MAX_DRAWIMAGES

static struct Vk_Image  s_vkImages[MAX_DRAWIMAGES];

#define MAX_IMAGE_CHUNKS        16
static struct Chunk s_ImageChunks[MAX_IMAGE_CHUNKS];
static int s_NumImageChunks = 0;



///////////////////////////////////////

// Host visible memory used to copy image data to device local memory.
static VkBuffer s_StagingBuffer;
static VkDeviceMemory s_MemStgBuf;
// pointer to mapped staging buffer
static unsigned char* s_pStgBuf = NULL;
static VkDeviceSize s_StagingBufferSize = 0;

////////////////////////////////////////



// Descriptors and Descriptor Sets
// A descriptor is a special opaque shader variable that shaders use
// to access buffer and image resources in an indirect fashion.
// It can be thought of as a "pointer" to a resource. 
// The Vulkan API allows these variables to be changed between 
// draw operations so that the shaders can access different resources
// for each draw.

// A descriptor set is called a "set" because it can refer to an array
// of homogenous resources that can be described with the same layout binding.
// one possible way to use multiple descriptors is to construct a 
// descriptor set with two descriptors, with each descriptor referencing
// a separate texture. Both textures are therefore available during a draw.
// A command in a command buffer could then select the texture to use by
// specifying the index of the desired texture.
// To describe a descriptor set, you use a descriptor set layout.

// Descriptor sets corresponding to bound texture images.
static VkDescriptorSet s_CurrentDescriptorSets[2] = {0};

VkDescriptorSet* getCurDescriptorSetsPtr(void)
{
    return s_CurrentDescriptorSets;
}



static void vk_free_chunk(void)
{
	int i = 0;
	for(i = 0; i < s_NumImageChunks; i++)
	{
		qvkFreeMemory(vk.device, s_ImageChunks[i].memory, NULL);
		memset(&s_ImageChunks[i], 0, sizeof(struct Chunk) );
	}
    s_NumImageChunks = 0;
}

static void vk_free_staging_buffer(void)
{
	if (s_MemStgBuf != VK_NULL_HANDLE)
    {
		qvkFreeMemory(vk.device, s_MemStgBuf, NULL);
        memset(&s_MemStgBuf, 0, sizeof(VkDeviceMemory));
    }

	if (s_StagingBuffer != VK_NULL_HANDLE)
    {
        qvkDestroyBuffer(vk.device, s_StagingBuffer, NULL);
        memset(&s_StagingBuffer, 0, sizeof(VkBuffer));
    }
    s_StagingBufferSize = 0;
	s_pStgBuf = NULL;
}




// outside of TR since it shouldn't be cleared during ref re-init
// the renderer front end should never modify glstate_t
//typedef struct {

int	s_CurTmu;
//	int			texEnv[2];
//	int			faceCulling;
//	unsigned long	glStateBits;
//} glstate_t;



void GL_Bind( image_t* pImage )
{
    static int	s_CurTextures[2];
	if ( s_CurTextures[s_CurTmu] != pImage->texnum )
    {
		s_CurTextures[s_CurTmu] = pImage->texnum;

        pImage->frameUsed = tr.frameCount;

		// VULKAN
		s_CurrentDescriptorSets[s_CurTmu] = s_vkImages[pImage->index].descriptor_set;
	}
}


static void allocate_and_bind_image_memory(VkImage image)
{
    int i = 0;
	VkMemoryRequirements memory_requirements;
	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if (memory_requirements.size > IMAGE_CHUNK_SIZE) {
		ri.Error(ERR_FATAL, "Vulkan: could not allocate memory, image is too large.");
	}

	struct Chunk* chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	uint64_t mask = (memory_requirements.alignment - 1);

	for (i = 0; i < s_NumImageChunks; i++)
 	{
		// ensure that memory region has proper alignment
		VkDeviceSize offset = (s_ImageChunks[i].used + mask) & (~mask);

		if (offset + memory_requirements.size <= IMAGE_CHUNK_SIZE)
		{
			chunk = &s_ImageChunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL)
    {
		if (s_NumImageChunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached");
		}

		VkMemoryAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = IMAGE_CHUNK_SIZE;
		alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceMemory memory;
		VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));

		chunk = &s_ImageChunks[s_NumImageChunks];
		s_NumImageChunks++;
		chunk->memory = memory;
		chunk->used = memory_requirements.size;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties)
{
    uint32_t i;
	for (i = 0; i < vk.devMemProperties.memoryTypeCount; i++)
    {
		if ( ((memory_type_bits & (1 << i)) != 0) && (vk.devMemProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
			return i;
		}
	}

	ri.Error(ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties");
	return -1;
}


static void ensure_staging_buffer_allocation(VkDeviceSize size)
{
    if(s_StagingBufferSize < size)
    {
        ri.Printf(PRINT_ALL, "ensure_staging_buffer_allocation: %ld\n", size);
        s_StagingBufferSize = size;

        if (s_StagingBuffer != VK_NULL_HANDLE)
        {
            qvkDestroyBuffer(vk.device, s_StagingBuffer, NULL);
            memset(&s_StagingBuffer, 0, sizeof(VkBuffer));
        }

        if (s_MemStgBuf != VK_NULL_HANDLE)
        {
            qvkUnmapMemory(vk.device, s_MemStgBuf);
            qvkFreeMemory(vk.device, s_MemStgBuf, NULL);
            memset(&s_MemStgBuf, 0, sizeof(VkDeviceMemory));
        }

        {
            // Vulkan supports two primary resource types: buffers and images. 
            // Resources are views of memory with associated formatting and 
            // dimensionality. Buffers are essentially unformatted arrays of
            // bytes whereas images contain format information, can be 
            // multidimensional and may have associated metadata.
            VkBufferCreateInfo buffer_desc;
            buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_desc.pNext = NULL;
            buffer_desc.flags = 0;
            buffer_desc.size = size;
            
            // Source buffers must have been created with the 
            // VK_BUFFER_USAGE_TRANSFER_SRC_BIT usage bit enabled and
            // destination buffers must have been created with the
            // VK_BUFFER_USAGE_TRANSFER_DST_BIT usage bit enabled.
            buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_desc.queueFamilyIndexCount = 0;
            buffer_desc.pQueueFamilyIndices = NULL;
            VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &s_StagingBuffer));
        }
        // To determine the memory requirements for a buffer resource

        {
            VkMemoryRequirements memory_requirements;
            qvkGetBufferMemoryRequirements(vk.device, s_StagingBuffer, &memory_requirements);

            uint32_t memory_type = find_memory_type(memory_requirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkMemoryAllocateInfo alloc_info;
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.pNext = NULL;
            alloc_info.allocationSize = memory_requirements.size;
            alloc_info.memoryTypeIndex = memory_type;
            VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &s_MemStgBuf));
        }
        
        VK_CHECK(qvkBindBufferMemory(vk.device, s_StagingBuffer, s_MemStgBuf, 0));

        void* data;
        VK_CHECK(qvkMapMemory(vk.device, s_MemStgBuf, 0, VK_WHOLE_SIZE, 0, &data));

        s_pStgBuf = (unsigned char *)data;
    }
}


static void vk_upload_image_data(VkImage image, uint32_t width, uint32_t height, const unsigned char* pPixels)
{

	VkBufferImageCopy regions[16];
	uint32_t curLevel = 0;

	uint32_t buffer_size = 0;

    while (1)
    {
		VkBufferImageCopy region;
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = curLevel;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
        region.imageOffset.y = 0;
		region.imageOffset.z = 0;

        region.imageExtent.width = width;
		region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        
		regions[curLevel] = region;
		curLevel++;

		buffer_size += width * height * 4;

		width >>= 1;
        height >>= 1;

        if ((width == 0) || (height == 0))
			break;
	}


	ensure_staging_buffer_allocation(buffer_size);
    memcpy(s_pStgBuf, pPixels, buffer_size);


    VkCommandBuffer cmd_buf;
    {
        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &cmd_buf));
    }


    {
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = NULL;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = NULL;
        VK_CHECK(qvkBeginCommandBuffer(cmd_buf, &begin_info));
    }


	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = s_StagingBuffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;
    
	qvkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    record_image_layout_transition(cmd_buf, image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // An application can copy buffer and image data using several methods
    // depending on the type of data transfer. Data can be copied between
    // buffer objects with vkCmdCopyBuffer and a portion of an image can 
    // be copied to another image with vkCmdCopyImage. Image data can also
    // be copied to and from buffer memory using vkCmdCopyImageToBuffer 
    // and vkCmdCopyBufferToImage. Image data can be blitted (with or 
    // without scaling and filtering) with vkCmdBlitImage. Multisampled 
    // images can be resolved to a non-multisampled image with vkCmdResolveImage.
    //
    // The following valid usage rules apply to all copy commands:
    // Copy commands must be recorded outside of a render pass instance.
    // 
    // The set of all bytes bound to all the source regions must not overlap
    // the set of all bytes bound to the destination regions.
    //
    // The set of all bytes bound to each destination region must not overlap
    // the set of all bytes bound to another destination region.
    //
    // Copy regions must be non-empty.
    // 
    // Regions must not extend outside the bounds of the buffer or image level,
    // except that regions of compressed images can extend as far as the 
    // dimension of the image level rounded up to a complete compressed texel block.
    
    // To copy data from a buffer object to an image object
    
    // command_buffer is the command buffer into which the command will be recorded.
    // s_StagingBuffer is the source buffer.
    // image is the destination image.
    // dstImageLayout is the layout of the destination image subresources.
    // curLevel is the number of regions to copy.
    // pRegions is a pointer to an array of VkBufferImageCopy structures
    // specifying the regions to copy.
    qvkCmdCopyBufferToImage(cmd_buf, s_StagingBuffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, curLevel, regions);

    record_image_layout_transition(cmd_buf, image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ////////
    
    VK_CHECK(qvkEndCommandBuffer(cmd_buf));

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));	
    VK_CHECK(qvkQueueWaitIdle(vk.queue));
    
	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &cmd_buf);
}


static void vk_uploadSingleImage(VkImage image, uint32_t width, uint32_t height, const unsigned char* pPixels)
{

    VkBufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;

    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;


    const uint32_t buffer_size = width * height * 4;

	ensure_staging_buffer_allocation(buffer_size);
    memcpy(s_pStgBuf, pPixels, buffer_size);

    VkCommandBuffer cmd_buf;
    {
        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &cmd_buf));
    }

    {
        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = NULL;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = NULL;
        VK_CHECK(qvkBeginCommandBuffer(cmd_buf, &begin_info));
    }

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = s_StagingBuffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;
    
	qvkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    record_image_layout_transition(cmd_buf, image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // An application can copy buffer and image data using several methods
    // depending on the type of data transfer. Data can be copied between
    // buffer objects with vkCmdCopyBuffer and a portion of an image can 
    // be copied to another image with vkCmdCopyImage. Image data can also
    // be copied to and from buffer memory using vkCmdCopyImageToBuffer 
    // and vkCmdCopyBufferToImage. Image data can be blitted (with or 
    // without scaling and filtering) with vkCmdBlitImage. Multisampled 
    // images can be resolved to a non-multisampled image with vkCmdResolveImage.
    //
    // The following valid usage rules apply to all copy commands:
    // Copy commands must be recorded outside of a render pass instance.
    // 
    // The set of all bytes bound to all the source regions must not overlap
    // the set of all bytes bound to the destination regions.
    //
    // The set of all bytes bound to each destination region must not overlap
    // the set of all bytes bound to another destination region.
    //
    // Copy regions must be non-empty.
    // 
    // Regions must not extend outside the bounds of the buffer or image level,
    // except that regions of compressed images can extend as far as the 
    // dimension of the image level rounded up to a complete compressed texel block.
    
    // To copy data from a buffer object to an image object
    
    // command_buffer is the command buffer into which the command will be recorded.
    // s_StagingBuffer is the source buffer.
    // image is the destination image.
    // dstImageLayout is the layout of the destination image subresources.
    // num_regions is the number of regions to copy.
    // pRegions is a pointer to an array of VkBufferImageCopy structures
    // specifying the regions to copy.
    qvkCmdCopyBufferToImage(cmd_buf, s_StagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    record_image_layout_transition(cmd_buf, image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ////////
    
    VK_CHECK(qvkEndCommandBuffer(cmd_buf));

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));	
    VK_CHECK(qvkQueueWaitIdle(vk.queue));
    
	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &cmd_buf);
}


void vk_destroyImageRes(void)
{
    vk_free_sampler();

	vk_free_staging_buffer();   

	vk_free_chunk();


	uint32_t i = 0;
	for (i = 0; i < MAX_DRAWIMAGES; i++)
    {

		if (s_vkImages[i].handle != VK_NULL_HANDLE)
        {
            //ri.Printf(PRINT_ALL, " Destroy VkImage s_vkImages[%d].{VkImage VkImageView} \n", i);
			qvkDestroyImage(vk.device, s_vkImages[i].handle, NULL);
			qvkDestroyImageView(vk.device, s_vkImages[i].view, NULL);
   		}

        if(s_vkImages[i].descriptor_set != VK_NULL_HANDLE)
        {   
            //ri.Printf(PRINT_ALL, " Free Descriptor Sets s_vkImages[%d].descriptor_set. \n", i);
            //To free allocated descriptor sets
            qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &s_vkImages[i].descriptor_set);
        }
	}
    
    memset(s_vkImages, 0, MAX_DRAWIMAGES*sizeof(struct Vk_Image));
	
    memset(s_CurrentDescriptorSets, 0,  2 * sizeof(VkDescriptorSet));

    R_resetGammaIntensityTable();
	
    VK_CHECK(qvkResetDescriptorPool(vk.device, vk.descriptor_pool, 0));
    
    ///////////////////////////////////
    // s_CurTextures[0] = s_CurTextures[1] = 0;
	s_CurTmu = 0;
}






#define FILE_HASH_SIZE	1024
static image_t*	hashTable[FILE_HASH_SIZE];


static int generateHashValue( const char *fname )
{
	int	i = 0;
	int	hash = 0;

	while (fname[i] != '\0') {
		char letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

void record_image_layout_transition( 
        VkCommandBuffer cmdBuf,
        VkImage image,
        VkImageAspectFlags image_aspect_flags,
        VkAccessFlags src_access_flags,
        VkImageLayout old_layout,
        VkAccessFlags dst_access_flags,
        VkImageLayout new_layout )
{

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access_flags;
	barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

// vkCmdPipelineBarrier is a synchronization command that 
// inserts a dependency between commands submitted to the
// same queue, or between commands in the same subpass.
// When vkCmdPipelineBarrier is submitted to a queue, 
// it defines a memory dependency between commands that
// were submitted before it, and those submitted after it.
    
    // cmdBuf is the command buffer into which the command is recorded.
	qvkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,	0, NULL, 0, NULL, 1, &barrier);
}

/*
================
This is the only way any image_t are created
================
*/

static void vk_createImageHandle(const uint32_t width, const uint32_t height, const uint32_t mipLevels, VkImage* pImage)
{
    VkImageCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.extent.width = width;
    desc.extent.height = height;
    desc.extent.depth = 1;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, pImage));
}

void vk_createImageView(VkImage h_image, VkImageView* pView)
{
    VkImageViewCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.image = h_image;
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;

    // the components field allows you to swizzle the color channels around
    desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // The subresourceRange field describes what the image's purpose is
    // and which part of the image should be accessed. 
    desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.subresourceRange.baseMipLevel = 0;
    desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;

    VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, pView));
}


void vk_createDescriptorSet(VkImageView imageView, VkSampler sampler, VkDescriptorSet* pDespSet)
{

	// create associated descriptor set
    // Allocate a descriptor set from the pool. 
    // Note that we have to provide the descriptor set layout that 
    // we defined in the pipeline_layout sample. 
    // This layout describes how the descriptor set is to be allocated.


    VkDescriptorSetAllocateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc.pNext = NULL;
    desc.descriptorPool = vk.descriptor_pool;
    desc.descriptorSetCount = 1;
    desc.pSetLayouts = &vk.set_layout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &desc, pDespSet));
    //ri.Printf(PRINT_ALL, " Allocate Descriptor Sets \n");


    VkDescriptorImageInfo image_info;
    {
        image_info.sampler = sampler;
        image_info.imageView = imageView;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet descriptor_write;
    {
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = *pDespSet;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pNext = NULL;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.pImageInfo = &image_info;
        descriptor_write.pBufferInfo = NULL;
        descriptor_write.pTexelBufferView = NULL;
    }

    qvkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, NULL);

    // The above steps essentially copy the VkDescriptorBufferInfo
    // to the descriptor, which is likely in the device memory.
    //
    // This buffer info includes the handle to the uniform buffer
    // as well as the offset and size of the data that is accessed
    // in the uniform buffer. In this case, the uniform buffer 
    // contains only the MVP transform, so the offset is 0 and 
    // the size is the size of the MVP.
}



image_t* R_CreateImage( const char *name, unsigned char* pic, uint32_t width, uint32_t height,
						VkBool32 isMipMap, VkBool32 allowPicmip, int glWrapClampMode )
{
	if (strlen(name) >= MAX_QPATH ) {
		ri.Error (ERR_DROP, "CreateImage: \"%s\" is too long\n", name);
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Error( ERR_DROP, "CreateImage: MAX_DRAWIMAGES hit\n");
	}

    ri.Printf( PRINT_ALL, "R_CreateImage: %s\n", name);
    
    // Create image_t object.
	image_t* pImage = tr.images[tr.numImages] = (image_t*) ri.Hunk_Alloc( sizeof( image_t ), h_low );
    pImage->index = tr.numImages;
	pImage->texnum = 1024 + tr.numImages;
	pImage->mipmap = isMipMap;
	pImage->allowPicmip = allowPicmip;
	strncpy (pImage->imgName, name, sizeof(pImage->imgName));
	pImage->width = width;
	pImage->height = height;
	pImage->wrapClampMode = glWrapClampMode;

	int hash = generateHashValue(name);
	pImage->next = hashTable[hash];
	hashTable[hash] = pImage;

	tr.numImages++;

	// Create corresponding GPU resource.
    // lightmaps are always allocated on TMU 1
	pImage->TMU = s_CurTmu = (strncmp(name, "*lightmap", 9) == 0);
	
    GL_Bind(pImage);

	// convert to exact power of 2 sizes
	unsigned int scaled_width, scaled_height;
    GetScaledDimension(width, height, &scaled_width, &scaled_height, allowPicmip);
    
    unsigned int nBytes = 4 * scaled_width * scaled_height;

	unsigned char* upload_buffer = (unsigned char*) malloc ( 2 * nBytes);


	if ( (scaled_width != width) || (scaled_height != height) )
    {
        ResampleTexture (pic, width, height, upload_buffer, scaled_width, scaled_height);
	}
    else
    {
        memcpy(upload_buffer, pic, nBytes);
    }


    // At this point width == scaled_width and height == scaled_height.

	const uint32_t base_width = scaled_width;
	const uint32_t base_height = scaled_height;
    uint32_t mipMapLevels = 1;

	if (isMipMap)
    {

        R_LightScaleTexture(upload_buffer, upload_buffer, nBytes);
        //go down from [width, height] to [scaled_width, scaled_height]

        unsigned char* in_buffer = upload_buffer;
        unsigned char* dst_ptr = in_buffer + nBytes;

        // Use the normal mip-mapping to go down from [scaled_width, scaled_height] to [1,1] dimensions.
        while (1)
        {

            if ( r_simpleMipMaps->integer )
            {
                R_MipMap(in_buffer, scaled_width, scaled_height, dst_ptr);
            }
            else
            {
                R_MipMap2(in_buffer, scaled_width, scaled_height, dst_ptr);
            }

            //ri.Printf( PRINT_WARNING, "%s, width: %d, height: %d, scaled_width: %d, scaled_height: %d\n",
            //name, width, height, scaled_width, scaled_height );

            scaled_width >>= 1;
            //if (scaled_width == 0)
            //    scaled_width = 1;

            scaled_height >>= 1;
            //if (scaled_height == 0)
            //    scaled_height = 1;
            
            if((scaled_width == 0) && (scaled_height == 0))
                break;


            uint32_t mip_level_size = scaled_width * scaled_height * 4;

            if ( r_colorMipLevels->integer ) {
                R_BlendOverTexture( in_buffer, scaled_width * scaled_height, mipMapLevels );
            }

            ++mipMapLevels;


            in_buffer = dst_ptr;
            dst_ptr += mip_level_size; 
        }
    }

    struct Vk_Image * pCurImg = &s_vkImages[pImage->index];

	vk_createImageHandle(base_width, base_height, mipMapLevels, &pCurImg->handle);

    allocate_and_bind_image_memory(pCurImg->handle);
	
    vk_createImageView(pCurImg->handle, &pCurImg->view);
    
    vk_createDescriptorSet(pCurImg->view ,vk_find_sampler(isMipMap, glWrapClampMode == GL_REPEAT),
            &pCurImg->descriptor_set);

    if(isMipMap)
        vk_upload_image_data(s_vkImages[pImage->index].handle, base_width, base_height, upload_buffer);
    else
        vk_uploadSingleImage(s_vkImages[pImage->index].handle, base_width, base_height, upload_buffer);

    free(upload_buffer);

    s_CurrentDescriptorSets[s_CurTmu] = pCurImg->descriptor_set;
	
    if (s_CurTmu) {
		s_CurTmu = 0;
	}

    return pImage;
}


image_t* R_FindImageFile(const char *name, VkBool32 mipmap, VkBool32 allowPicmip, int glWrapClampMode)
{
   	image_t* image;

	if (name == NULL)
    {
        ri.Printf( PRINT_WARNING, "R_FindImageFile: NULL\n");
		return NULL;
	}

	int hash = generateHashValue(name);

	// see if the image is already loaded

	for (image=hashTable[hash]; image; image=image->next)
	{
		if ( !strcmp( name, image->imgName ) )
		{
			// the white image can be used with any set of parms,
			// but other mismatches are errors
			if ( strcmp( name, "*white" ) )
			{
				if ( image->mipmap != mipmap ) {
					ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed mipmap parm\n", name );
				}
				if ( image->allowPicmip != allowPicmip ) {
					ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed allowPicmip parm\n", name );
				}
				if ( image->wrapClampMode != glWrapClampMode ) {
					ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed glWrapClampMode parm\n", name );
				}
			}
			return image;
		}
	}

	//
	// load the pic from disk
    //
    uint32_t width, height;
	unsigned char* pic;
    R_LoadImage2( name, &pic, &width, &height );
	if (pic == NULL)
	{
        ri.Printf( PRINT_WARNING,
            "R_FindImageFile: Fail loading %s the from disk\n", name);
        return NULL;
    }

	image = R_CreateImage( name, pic, width, height, mipmap, allowPicmip, glWrapClampMode );

    ri.Free( pic );
    
	return image;
}


void RE_UploadCinematic (int w, int h, int cols, int rows, const unsigned char *data, int client, VkBool32 dirty)
{

	// GL_Bind( tr.scratchImage[client] );
    image_t* prtImage = tr.scratchImage[client];
    struct Vk_Image* pImage = &s_vkImages[prtImage->index];
	
    // if the scratchImage isn't in the format we want, specify it as a new texture
    if ( (cols != prtImage->width) || (rows != prtImage->height) )
    {
        ri.Printf(PRINT_ALL, "w=%d, h=%d, cols=%d, rows=%d, client=%d, prtImage->width=%d\n, prtImage->height=%d", 
           w, h, cols, rows, client, prtImage->width, prtImage->height);

        prtImage->width = cols;
        prtImage->height = rows;

        // VULKAN

        qvkDestroyImage(vk.device, pImage->handle, NULL);
        qvkDestroyImageView(vk.device, pImage->view, NULL);
        qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &pImage->descriptor_set);
        
        vk_createImageHandle(cols, rows, 1, &pImage->handle);

        allocate_and_bind_image_memory(pImage->handle);
	
        vk_createImageView(pImage->handle, &pImage->view);
    
        vk_createDescriptorSet(pImage->view , vk_find_sampler(0, 0), &pImage->descriptor_set);
        
        vk_uploadSingleImage(pImage->handle, cols, rows, data);
    }
    else if (dirty)
    {
        // otherwise, just subimage upload it so that
        // drivers can tell we are going to be changing
        // it and don't try and do a texture compression       
        vk_uploadSingleImage(pImage->handle, cols, rows, data);
    }

}


void R_InitImages( void )
{
    memset(hashTable, 0, sizeof(hashTable));
    // important
    R_resetGammaIntensityTable();

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}
