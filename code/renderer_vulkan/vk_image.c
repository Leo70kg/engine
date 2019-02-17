#include "tr_local.h"
#include "vk_image_sampler.h"
#include "R_ImageProcess.h"
#include "vk_image.h"
#include "vk_cmd.h"
#include "vk_instance.h"
#include "tr_globals.h"
#include "tr_cvar.h"


#define IMAGE_CHUNK_SIZE        (192 * 1024 * 1024)

// 2 for mipmap, 4 for RGBA, 1024 width
#define MAX_IMAGE_SIZE        (2 * 4 * 1024 * 1024)

/*

Images created with tiling equal to VK_IMAGE_TILING_LINEAR have further restrictions on their
limits and capabilities compared to images created with tiling equal to VK_IMAGE_TILING_OPTIMAL.
Creation of images with tiling VK_IMAGE_TILING_LINEAR may not be supported unless other parameters
meetall of the constraints:
* imageType is VK_IMAGE_TYPE_2D
* format is not a depth/stencil format
* mipLevels is 1
* arrayLayers is 1
* samples is VK_SAMPLE_COUNT_1_BIT
* usage only includes VK_IMAGE_USAGE_TRANSFER_SRC_BIT and/or VK_IMAGE_USAGE_TRANSFER_DST_BIT
Implementations may support additional limits and capabilities beyond those listed above.

*/

struct StagingBufferImage
{
    // Vulkan supports two primary resource types: buffers and images. 
    // Resources are views of memory with associated formatting and dimensionality.
    // Buffers are essentially unformatted arrays of bytes whereas images contain
    // format information, can be multidimensional and may have associated metadata.
    //
    // Buffers represent linear arrays of data which are used for various purposes
    // by binding them to a graphics or compute pipeline via descriptor sets or via
    // certain commands, or by directly specifying them as parameters to certain commands.
    VkBuffer buff;

    // Host visible memory used to copy image data to device local memory.
    VkDeviceMemory devMemMappable;

    // One large device device local memory allocation, assigned to multiple images
    VkDeviceMemory devMemStoreImg;
    uint32_t memUsed;

// pointer to mapped staging buffer
//    unsigned char* pBufMapped;
    VkDeviceSize buf_size;
};


struct StagingBufferImage StagImg;

void gpuMemUsageInfo_f(void)
{
    ri.Printf(PRINT_ALL, "Staging buffer size: %ld\n", StagImg.buf_size );
        // 	for debug info
    ri.Printf(PRINT_ALL, "Image chuck memory(device local) used: %dM,%dK Bytes\n", 
            StagImg.memUsed / (1024 * 1024), (StagImg.memUsed % (1024 * 1024))/1024);
}


////////////////////////////////////////

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


static void vk_createStagingBuffer(uint32_t size)
{
    ri.Printf(PRINT_ALL, " Create Staging Buffer: %d\n", size);

    {
        VkBufferCreateInfo buffer_desc;
        buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_desc.pNext = NULL;
        // flags is a bitmask of VkBufferCreateFlagBits specifying additional parameters of the buffer.
        buffer_desc.flags = 0;
        buffer_desc.size = size;
        // VK_BUFFER_USAGE_TRANSFER_SRC_BIT specifies that the buffer
        // can be used as the source of a transfer command
        buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // sharingMode is a VkSharingMode value specifying the sharing mode of the buffer 
        // when it will be accessed by multiple queue families.
        buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // queueFamilyIndexCount is the number of entries in the pQueueFamilyIndices array.
        buffer_desc.queueFamilyIndexCount = 0;
        // pQueueFamilyIndices is a list of queue families that will access this buffer
        // (ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT).
        buffer_desc.pQueueFamilyIndices = NULL;

        VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &StagImg.buff));

        StagImg.buf_size = size;
    }

    // To determine the memory requirements for a buffer resource

    //  typedef struct VkMemoryRequirements {
    //  VkDeviceSize size;
    //  VkDeviceSize alignment;
    //  uint32_t memoryTypeBits;
    //  } VkMemoryRequirements;

    {
        VkMemoryRequirements memory_requirements;
        qvkGetBufferMemoryRequirements(vk.device, StagImg.buff, &memory_requirements);

        VkMemoryAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &StagImg.devMemMappable));

        VK_CHECK(qvkBindBufferMemory(vk.device, StagImg.buff, StagImg.devMemMappable, 0));

        ri.Printf(PRINT_ALL, " Stagging buffer alignment: %ld, memoryTypeBits: 0x%x, Type Index: %d. \n",
            memory_requirements.alignment, memory_requirements.memoryTypeBits, alloc_info.memoryTypeIndex);
    }


    // ======================================================
    // Allocate a large device local memory chunk in advance
    // ======================================================
    ri.Printf(PRINT_WARNING, " Allocate large device local memory chunk for storing verious images.\n");

    // create a dummy image
    // Images represent multidimensional - up to 3 - arrays of data 
    // which can be used for various purposes (e.g. attachments, textures),
    // by binding them to a graphics or compute pipeline via descriptor sets, 
    // or by directly specifying them as parameters to certain commands.
    VkImage dummy;


    VkImageCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    desc.pNext = NULL;
    // flags is a bitmask of VkImageCreateFlagBits describing additional parameters of the image.
    desc.flags = 0;
    // imageType is a VkImageType value specifying the basic dimensionality of the image.
    // Layers in array textures do not count as a dimension for the purposes of the image type
    desc.imageType = VK_IMAGE_TYPE_2D;
    // format is a VkFormat describing the format and type of the data elements
    // that will be contained in the image.
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    // extent is a VkExtent3D describing the number of data elements in each dimension of the base level.
    desc.extent.width = 1920;
    desc.extent.height = 1080;
    desc.extent.depth = 1;
    // mipLevels describes the number of levels of detail available for minified sampling of the image
    desc.mipLevels = 1;
    // arrayLayers is the number of layers in the image.
    desc.arrayLayers = 1;
    // samples is the number of sub-data element samples in the image as defined
    // in VkSampleCountFlagBits. See Multisampling.
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    // tiling is a VkImageTiling value specifying the tiling arrangement of the data elements in memory.
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    // usage is a bitmask of VkImageUsageFlagBits describing the intended usage of the image.
    desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // sharingMode is a VkSharingMode value specifying the sharing mode of the image
    // when it will be accessed by multiple queue families.
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    // initialLayout is a VkImageLayout value specifying the initial VkImageLayout
    // of all image subresources of the image. See Image Layouts.
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &dummy));

    
    VkMemoryRequirements imageMemoryRequirements;
    qvkGetImageMemoryRequirements(vk.device, dummy, &imageMemoryRequirements);
    

    qvkDestroyImage(vk.device, dummy, NULL);

    VkMemoryAllocateInfo dev_local_alloc_info;
    dev_local_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    dev_local_alloc_info.pNext = NULL;
    dev_local_alloc_info.allocationSize = IMAGE_CHUNK_SIZE;
    dev_local_alloc_info.memoryTypeIndex = find_memory_type(imageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
       
    VK_CHECK(qvkAllocateMemory( vk.device, &dev_local_alloc_info, NULL, &StagImg.devMemStoreImg ) );
    
    StagImg.memUsed = 0;

    ri.Printf(PRINT_ALL, " Device local memory for store image: alignment = %ld, Type Bits = 0x%x, Type Index = %d\n",
            imageMemoryRequirements.alignment, imageMemoryRequirements.memoryTypeBits, dev_local_alloc_info.memoryTypeIndex);

}


static void vk_destroy_staging_buffer(void)
{
    if (StagImg.buff != VK_NULL_HANDLE)
    {
        qvkDestroyBuffer(vk.device, StagImg.buff, NULL);
    }
    
    if (StagImg.devMemMappable != VK_NULL_HANDLE)
    {
        qvkFreeMemory(vk.device, StagImg.devMemMappable, NULL);
    }

    if (StagImg.devMemStoreImg != VK_NULL_HANDLE)
    {
        qvkFreeMemory(vk.device, StagImg.devMemStoreImg, NULL);
    }

    memset(&StagImg, 0, sizeof(StagImg));
}





static void vk_stagBufferToDeviceLocalMem(VkImage image, VkBufferImageCopy* pRegion, uint32_t num_region)
{
    // An application can copy buffer and image data using several methods
    // depending on the type of data transfer. Data can be copied between
    // buffer objects with vkCmdCopyBuffer and a portion of an image can 
    // be copied to another image with vkCmdCopyImage. 
    //
    // Image data can also be copied to and from buffer memory using
    // vkCmdCopyImageToBuffer and vkCmdCopyBufferToImage.
    //
    // Image data can be blitted (with or without scaling and filtering) 
    // with vkCmdBlitImage. Multisampled images can be resolved to a 
    // non-multisampled image with vkCmdResolveImage.
    //
    VkCommandBuffer cmd_buf;
    {
        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &cmd_buf));

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
	barrier.buffer = StagImg.buff;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;
    
	qvkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    record_image_layout_transition(cmd_buf, image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);


    
    // To copy data from a buffer object to an image object
    
    // cmd_buf is the command buffer into which the command will be recorded.
    // StagImg.buff is the source buffer.
    // image is the destination image.
    // dstImageLayout is the layout of the destination image subresources.
    // curLevel is the number of regions to copy.
    // pRegions is a pointer to an array of VkBufferImageCopy structures
    // specifying the regions to copy.
    qvkCmdCopyBufferToImage(cmd_buf, StagImg.buff, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_region, pRegion);

    record_image_layout_transition(cmd_buf, image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    
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


#define FILE_HASH_SIZE	1024
static image_t*	hashTable[FILE_HASH_SIZE];


static int generateHashValue( const char *fname )
{
	uint32_t i = 0;
	int	hash = 0;

	while (fname[i] != '\0') {
		char letter = tolower(fname[i]);
		if (letter =='.') break;		// don't include extension
		if (letter =='\\') letter = '/';	// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}


/*
================
This is the only way any image_t are created
================
*/
static void vk_createImageAndBindWithMemory(image_t* pImg)
{

    VkImageCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.imageType = VK_IMAGE_TYPE_2D;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.extent.width = pImg->uploadWidth;
    desc.extent.height = pImg->uploadHeight;
    desc.extent.depth = 1;
    desc.mipLevels = pImg->mipLevels;
    desc.arrayLayers = 1;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &pImg->handle));

    // =======================================================
    // Binding it with device local memory
    // =======================================================
    VkMemoryRequirements memory_requirements;
    qvkGetImageMemoryRequirements(vk.device, pImg->handle, &memory_requirements);
    
    const uint32_t mask = (memory_requirements.alignment - 1);

    // ensure that memory region has proper alignment
    const uint32_t offset_aligned = (StagImg.memUsed + mask) & (~mask);
    
    StagImg.memUsed = offset_aligned + memory_requirements.size;

    // ensure that device local memory is enough
    assert (StagImg.memUsed <= IMAGE_CHUNK_SIZE);


    // To attach memory to a VkImage object created without the VK_IMAGE_CREATE_DISJOINT_BIT set
    //
    // StagImg.devMemStoreImg is the VkDeviceMemory object describing the device memory to attach.
    // offset_aligned is the start offset of the region of memory which is to be bound to the image. 
    // The number of bytes returned in the VkMemoryRequirements::size member in memory,
    // starting from memoryOffset bytes, will be bound to the specified image.

    VK_CHECK(qvkBindImageMemory(vk.device, pImg->handle, StagImg.devMemStoreImg, offset_aligned));
    // 	for debug info
    //  ri.Printf(PRINT_ALL, "GPU image chuck memory Used = %dM,%dK Bytes\n", 
    //        StagImg.memUsed / (1024 * 1024), (StagImg.memUsed % (1024 * 1024))/1024);
    // return this_image;
}



static void vk_createImageViewAndDescriptorSet(image_t* pImage)
{

    VkImageView imageView;

    VkImageViewCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.image = pImage->handle;
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    // format is a VkFormat describing the format and type used 
    // to interpret data elements in the image.
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    
    // the components field allows you to swizzle the color channels around
    desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // The subresourceRange field describes what the image's purpose is
    // and which part of the image should be accessed. 
    //
    // selecting the set of mipmap levels and array layers to be accessible to the view.
    desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.subresourceRange.baseMipLevel = 0;
    desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;
    // Image objects are not directly accessed by pipeline shaders for reading or writing image data.
    // Instead, image views representing contiguous ranges of the image subresources and containing
    // additional metadata are used for that purpose. Views must be created on images of compatible
    // types, and must represent a valid subset of image subresources.
    //
    // Some of the image creation parameters are inherited by the view. In particular, image view
    // creation inherits the implicit parameter usage specifying the allowed usages of the image 
    // view that, by default, takes the value of the corresponding usage parameter specified in
    // VkImageCreateInfo at image creation time.
    //
    // This implicit parameter can be overriden by chaining a VkImageViewUsageCreateInfo structure
    // through the pNext member to VkImageViewCreateInfo.
    VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &imageView));

    /////  save it just for destroy ???
    pImage->view = imageView;


    ///////////////////////////////////////////////////////
    // create associated descriptor set
    ///////////////////////////////////////////////////////
    // Allocate a descriptor set from the pool. 
    // Note that we have to provide the descriptor set layout that 
    // we defined in the pipeline_layout sample. 
    // This layout describes how the descriptor set is to be allocated.

    VkDescriptorSet desSet;

    VkDescriptorSetAllocateInfo descSetAllocInfo;
    descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAllocInfo.pNext = NULL;
    descSetAllocInfo.descriptorPool = vk.descriptor_pool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts = &vk.set_layout;

    VK_CHECK(qvkAllocateDescriptorSets(vk.device, &descSetAllocInfo, &desSet));
    
    /////  save it for destroy and update current descriptor
    pImage->descriptor_set = desSet;

    //ri.Printf(PRINT_ALL, " Allocate Descriptor Sets \n");
    VkWriteDescriptorSet descriptor_write;

    VkDescriptorImageInfo image_info;
    image_info.sampler = vk_find_sampler(pImage->mipmap, pImage->wrapClampMode == GL_REPEAT);
    image_info.imageView = imageView;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = desSet;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pNext = NULL;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &image_info;
    descriptor_write.pBufferInfo = NULL;
    descriptor_write.pTexelBufferView = NULL;
    

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



image_t* R_CreateImage( const char *name, unsigned char* pic, const uint32_t width, const uint32_t height,
						VkBool32 isMipMap, VkBool32 allowPicmip, int glWrapClampMode, VkBool32 isAlone )
{
    if (strlen(name) >= MAX_QPATH ) {
        ri.Error (ERR_DROP, "CreateImage: \"%s\" is too long\n", name);
    }

    if ( tr.numImages == MAX_DRAWIMAGES ) {
        ri.Error( ERR_DROP, "CreateImage: MAX_DRAWIMAGES hit\n");
    }

    //ri.Printf( PRINT_ALL, "R_CreateImage: %s\n", name);
    
    // Create image_t object.

    image_t* pImage = (image_t*) ri.Hunk_Alloc( sizeof( image_t ), h_low );

    strncpy (pImage->imgName, name, sizeof(pImage->imgName));
    pImage->index = tr.numImages;
    pImage->mipmap = isMipMap;
    pImage->mipLevels = 1;
    pImage->allowPicmip = allowPicmip;
    pImage->wrapClampMode = glWrapClampMode;
    pImage->width = width;
    pImage->height = height;
    pImage->isLightmap = (strncmp(name, "*lightmap", 9) == 0);
    // Create corresponding GPU resource, lightmaps are always allocated on TMU 1 .
    // A texture mapping unit (TMU) is a component in modern graphics processing units (GPUs). 
    // Historically it was a separate physical processor. A TMU is able to rotate, resize, 
    // and distort a bitmap image (performing texture sampling), to be placed onto an arbitrary
    // plane of a given 3D model as a texture. This process is called texture mapping. 
    // In modern graphics cards it is implemented as a discrete stage in a graphics pipeline, 
    // whereas when first introduced it was implemented as a separate processor, 
    // e.g. as seen on the Voodoo2 graphics card. 
    //
    // The TMU came about due to the compute demands of sampling and transforming a flat
    // image (as the texture map) to the correct angle and perspective it would need to
    // be in 3D space. The compute operation is a large matrix multiply, 
    // which CPUs of the time (early Pentiums) could not cope with at acceptable performance.
    //
    // Today (2013), TMUs are part of the shader pipeline and decoupled from the
    // Render Output Pipelines (ROPs). For example, in AMD's Cypress GPU, 
    // each shader pipeline (of which there are 20) has four TMUs, giving the GPU 80 TMUs.
    // This is done by chip designers to closely couple shaders and the texture engines
    // they will be working with. 
    //
    // 3D scenes are generally composed of two things: 3D geometry, and the textures 
    // that cover that geometry. Texture units in a video card take a texture and 'map' it
    // to a piece of geometry. That is, they wrap the texture around the geometry and 
    // produce textured pixels which can then be written to the screen. 
    //
    // Textures can be an actual image, a lightmap, or even normal maps for advanced 
    // surface lighting effects. 


    // convert to exact power of 2 sizes
    GetScaledDimension(width, height, &pImage->uploadWidth, &pImage->uploadHeight, allowPicmip);
  

    uint32_t buffer_size = 4 * pImage->uploadWidth * pImage->uploadHeight;
    unsigned char * const upload_buffer = (unsigned char*) malloc ( 2 * buffer_size);

    if ( (pImage->uploadWidth != width) || (pImage->uploadHeight != height) )
    {
        // just info
        ri.Printf( PRINT_WARNING, "ResampleTexture: inwidth: %d, inheight: %d, outwidth: %d, outheight: %d\n",
                width, height, pImage->uploadWidth, pImage->uploadHeight );
        
        //go down from [width, height] to [scaled_width, scaled_height]
        ResampleTexture (upload_buffer, width, height, pic, pImage->uploadWidth, pImage->uploadHeight);
    }
    else
    {
        memcpy(upload_buffer, pic, buffer_size);
    }

    ////////////////////////////////////////////////////////////////////
    // 2^12 = 4096
    // The set of all bytes bound to all the source regions must not overlap
    // the set of all bytes bound to the destination regions.
    //
    // The set of all bytes bound to each destination region must not overlap
    // the set of all bytes bound to another destination region.

    VkBufferImageCopy regions[12];

    regions[0].bufferOffset = 0;
    regions[0].bufferRowLength = 0;
    regions[0].bufferImageHeight = 0;
    regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    regions[0].imageSubresource.mipLevel = 0;
    regions[0].imageSubresource.baseArrayLayer = 0;
    regions[0].imageSubresource.layerCount = 1;
    regions[0].imageOffset.x = 0;
    regions[0].imageOffset.y = 0;
    regions[0].imageOffset.z = 0;
    regions[0].imageExtent.width = pImage->uploadWidth;
    regions[0].imageExtent.height = pImage->uploadHeight;
    regions[0].imageExtent.depth = 1;


    if(isMipMap)
    {
        uint32_t curMipMapLevel = 1; 
        uint32_t base_width = pImage->uploadWidth;
        uint32_t base_height = pImage->uploadHeight;

        unsigned char* in_ptr = upload_buffer;
        unsigned char* dst_ptr = in_ptr + buffer_size;

        R_LightScaleTexture(upload_buffer, upload_buffer, buffer_size);

        // Use the normal mip-mapping to go down from [scaled_width, scaled_height] to [1,1] dimensions.

        while (1)
        {

            if ( r_simpleMipMaps->integer )
            {
                R_MipMap(in_ptr, base_width, base_height, dst_ptr);
            }
            else
            {
                R_MipMap2(in_ptr, base_width, base_height, dst_ptr);
            }


            if ((base_width == 1) && (base_height == 1))
                break;

            base_width >>= 1;
            if (base_width == 0) 
                base_width = 1;

            base_height >>= 1;
            if (base_height == 0)
                base_height = 1;

            regions[curMipMapLevel].bufferOffset = buffer_size;
            regions[curMipMapLevel].bufferRowLength = 0;
            regions[curMipMapLevel].bufferImageHeight = 0;
            regions[curMipMapLevel].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[curMipMapLevel].imageSubresource.mipLevel = curMipMapLevel;
            regions[curMipMapLevel].imageSubresource.baseArrayLayer = 0;
            regions[curMipMapLevel].imageSubresource.layerCount = 1;
            regions[curMipMapLevel].imageOffset.x = 0;
            regions[curMipMapLevel].imageOffset.y = 0;
            regions[curMipMapLevel].imageOffset.z = 0;

            regions[curMipMapLevel].imageExtent.width = base_width;
            regions[curMipMapLevel].imageExtent.height = base_height;
            regions[curMipMapLevel].imageExtent.depth = 1;
            

            uint32_t curLevelSize = base_width * base_height * 4;

            buffer_size += curLevelSize;
            
            // Regions must not extend outside the bounds of the buffer or image level,
            // except that regions of compressed images can extend as far as the
            // dimension of the image level rounded up to a complete compressed texel block.

            assert(buffer_size <= StagImg.buf_size);

            if ( r_colorMipLevels->integer ) {
                R_BlendOverTexture( in_ptr, base_width * base_height, curMipMapLevel );
            }


            ++curMipMapLevel;

            in_ptr = dst_ptr;
            dst_ptr += curLevelSize; 
        }
        pImage->mipLevels = curMipMapLevel; 
        // ri.Printf( PRINT_WARNING, "curMipMapLevel: %d, base_width: %d, base_height: %d, buffer_size: %d, name: %s\n",
        //    curMipMapLevel, scaled_width, scaled_height, buffer_size, name);
    }

    
    vk_createImageAndBindWithMemory(pImage);
    vk_createImageViewAndDescriptorSet(pImage);


    void* data;
    VK_CHECK(qvkMapMemory(vk.device, StagImg.devMemMappable, 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, upload_buffer, buffer_size);
    qvkUnmapMemory(vk.device, StagImg.devMemMappable);
    free(upload_buffer);

    vk_stagBufferToDeviceLocalMem(pImage->handle, regions, pImage->mipLevels);

    const int hash = generateHashValue(name);
    pImage->next = hashTable[hash];
    hashTable[hash] = pImage;


    if(!isAlone)
    {        
        tr.images[tr.numImages] = pImage;
        tr.numImages++;
    }

    return pImage;
}



image_t* R_FindImageFile(const char *name, VkBool32 mipmap, VkBool32 allowPicmip, int glWrapClampMode)
{
   	image_t* image;

	if (name == NULL)
    {
        ri.Printf( PRINT_WARNING, "Find Image File: NULL\n");
		return NULL;
	}
    // ri.Printf( PRINT_WARNING, "Find Image File: %s\n", name);

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
    uint32_t width = 0, height = 0;
    unsigned char* pic = NULL;
    R_LoadImage2( name, &pic, &width, &height );
    if (pic == NULL)
    {
        ri.Printf( PRINT_WARNING, "R_FindImageFile: Fail loading %s the from disk\n", name);
        return NULL;
    }

    image = R_CreateImage( name, pic, width, height, mipmap, allowPicmip, glWrapClampMode, VK_FALSE);

    ri.Free( pic );
    
    return image;
}



void RE_UploadCinematic (int w, int h, int cols, int rows, const unsigned char *data, int client, VkBool32 dirty)
{

    image_t* prtImage = tr.scratchImage[client];
    
    // if the scratchImage isn't in the format we want, specify it as a new texture
    if ( (cols != prtImage->uploadWidth) || (rows != prtImage->uploadHeight) )
    {
        ri.Printf(PRINT_ALL, "w=%d, h=%d, cols=%d, rows=%d, client=%d, prtImage->width=%d, prtImage->height=%d\n", 
           w, h, cols, rows, client, prtImage->uploadWidth, prtImage->uploadHeight);

        prtImage->uploadWidth = cols;
        prtImage->uploadHeight = rows;
        prtImage->mipLevels = 1; 
        // VULKAN

        qvkDestroyImage(vk.device, prtImage->handle, NULL);
        qvkDestroyImageView(vk.device, prtImage->view, NULL);
        qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &prtImage->descriptor_set);
        
        vk_createImageAndBindWithMemory(prtImage);

        //vk_createImageView(prtImage->handle, &prtImage->view);
        //vk_createDescriptorSet(prtImage->view , vk_find_sampler(0, 0), &prtImage->descriptor_set);
        // vk_find_sampler(0, 0)
        vk_createImageViewAndDescriptorSet(prtImage);
        //vk_find_sampler(isMipMap, glWrapClampMode == GL_REPEAT)

        // vk_uploadSingleImage(prtImage->handle, cols, rows, data);

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
        region.imageExtent.width = cols;
        region.imageExtent.height = rows;
        region.imageExtent.depth = 1;

        const uint32_t buffer_size = cols * rows * 4;

        void* pDat;
        VK_CHECK(qvkMapMemory(vk.device, StagImg.devMemMappable, 0, VK_WHOLE_SIZE, 0, &pDat));
        memcpy(pDat, data, buffer_size);
        qvkUnmapMemory(vk.device, StagImg.devMemMappable);

        vk_stagBufferToDeviceLocalMem(tr.scratchImage[client]->handle, &region, 1);

    }
    else if (dirty)
    {
        // otherwise, just subimage upload it so that
        // drivers can tell we are going to be changing
        // it and don't try and do a texture compression       
        // vk_uploadSingleImage(prtImage->handle, cols, rows, data);

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
        region.imageExtent.width = cols;
        region.imageExtent.height = rows;
        region.imageExtent.depth = 1;

        const uint32_t buffer_size = cols * rows * 4;

        void* pDat;
        VK_CHECK(qvkMapMemory(vk.device, StagImg.devMemMappable, 0, VK_WHOLE_SIZE, 0, &pDat));
        memcpy(pDat, data, buffer_size);
        qvkUnmapMemory(vk.device, StagImg.devMemMappable);

        vk_stagBufferToDeviceLocalMem(tr.scratchImage[client]->handle, &region, 1);
    }
}


static void R_CreateDefaultImage( void )
{
	#define	DEFAULT_SIZE	16

	unsigned char data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	memset( data, 32, sizeof( data ) );

	uint32_t x;
	for ( x = 0; x < DEFAULT_SIZE; x++ )
	{
		data[0][x][0] =
			data[0][x][1] =
			data[0][x][2] =
			data[0][x][3] = 255;

		data[x][0][0] =
			data[x][0][1] =
			data[x][0][2] =
			data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
			data[DEFAULT_SIZE-1][x][1] =
			data[DEFAULT_SIZE-1][x][2] =
			data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
			data[x][DEFAULT_SIZE-1][1] =
			data[x][DEFAULT_SIZE-1][2] =
			data[x][DEFAULT_SIZE-1][3] = 255;
	}
	tr.defaultImage = R_CreateImage("*default", (unsigned char *)data, DEFAULT_SIZE, DEFAULT_SIZE, qtrue, qfalse, GL_REPEAT, VK_TRUE);
}



static void R_CreateWhiteImage(void)
{
	// we use a solid white image instead of disabling texturing
	unsigned char data[DEFAULT_SIZE][DEFAULT_SIZE][4];
	memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImage("*white", (unsigned char *)data, 8, 8, qfalse, qfalse, GL_REPEAT, VK_TRUE);
}


static void R_CreateIdentityLightImage(void)
{
    uint32_t x,y;
	unsigned char data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;
		}
	}
	tr.identityLightImage = R_CreateImage("*identityLight", (unsigned char *)data, 8, 8, qfalse, qfalse, GL_REPEAT, VK_FALSE );
}


static void R_CreateDlightImage( void )
{
    #define	DLIGHT_SIZE	16

	uint32_t x,y;
	unsigned char data[DLIGHT_SIZE][DLIGHT_SIZE][4];

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x=0; x<DLIGHT_SIZE; x++)
    {
		for (y=0; y<DLIGHT_SIZE; y++)
        {
			float d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
				( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
			int b = 4000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}

			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = b;
			data[y][x][3] = 255;			
		}
	}
	tr.dlightImage = R_CreateImage("*dlight", (unsigned char *)data, DLIGHT_SIZE, DLIGHT_SIZE, qfalse, qfalse, GL_CLAMP, VK_TRUE );
}


static void R_CreateFogImage( void )
{
    #define	FOG_S	256
    #define	FOG_T	32

	unsigned int x,y;

	unsigned char* data = (unsigned char*) malloc( FOG_S * FOG_T * 4 );

	// S is distance, T is depth
	for (x=0 ; x<FOG_S ; x++)
    {
		for (y=0 ; y<FOG_T ; y++)
        {
			float d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );

            unsigned int index = (y*FOG_S+x)*4;
			data[index ] = 
			data[index+1] = 
			data[index+2] = 255;
			data[index+3] = 255*d;
		}
	}
	// standard openGL clamping doesn't really do what we want -- it includes
	// the border color at the edges.  OpenGL 1.2 has clamp-to-edge, which does
	// what we want.
	tr.fogImage = R_CreateImage("*fog", (unsigned char *)data, FOG_S, FOG_T, qfalse, qfalse, GL_CLAMP, VK_TRUE );
	
    free( data );
}

static void R_CreateScratchImage(void)
{
    uint32_t x;
    
    unsigned char data[DEFAULT_SIZE][DEFAULT_SIZE][4];

    for(x=0;x<32;x++)
    {
        // scratchimage is usually used for cinematic drawing
        tr.scratchImage[x] = R_CreateImage("*scratch", (unsigned char *)data, DEFAULT_SIZE, DEFAULT_SIZE, qfalse, qtrue, GL_CLAMP, VK_TRUE);
    }
}



void R_InitImages( void )
{
    memset(hashTable, 0, sizeof(hashTable));
    
    vk_createStagingBuffer(4 * 2048 * 2048);

    // build brightness translation tables
    R_SetColorMappings();

    // create default texture and white texture
    // R_CreateBuiltinImages();

    R_CreateDefaultImage();

    R_CreateWhiteImage();

    R_CreateIdentityLightImage();

    R_CreateScratchImage();
    
    R_CreateDlightImage();
    
    R_CreateFogImage();
}


static void R_DestroySingleImage( image_t* pImg )
{
    if (pImg->handle != VK_NULL_HANDLE)
    {
        qvkDestroyImageView(vk.device, pImg->view, NULL);
        qvkDestroyImage(vk.device, pImg->handle, NULL);
        pImg->handle = VK_NULL_HANDLE;
    }

    if(pImg->descriptor_set != VK_NULL_HANDLE)
    {   
        //ri.Printf(PRINT_ALL, " Free Descriptor Sets s_vkImages[%d].descriptor_set. \n", i);
        //To free allocated descriptor sets
        qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &pImg->descriptor_set);
        pImg->descriptor_set = VK_NULL_HANDLE;
    }
}


void vk_destroyImageRes(void)
{
	vk_free_sampler();

	vk_destroy_staging_buffer();   

	ri.Printf(PRINT_ALL, " tr.images[1:%d].{VkImage VkImageView descriptor_set} \n", tr.numImages);

	uint32_t i = 0;
	for (i = 0; i < tr.numImages; i++)
	{
        R_DestroySingleImage(tr.images[i]);
	}

/*
    R_DestroySingleImage(tr.identityLightImage);
*/
    R_DestroySingleImage(tr.defaultImage);
    R_DestroySingleImage(tr.whiteImage);
    R_DestroySingleImage(tr.fogImage);
    R_DestroySingleImage(tr.dlightImage);

    for(i=0; i<32; i++)
    {
		// scratchimage is usually used for cinematic drawing
		R_DestroySingleImage(tr.scratchImage[i]);
	}

    // Destroying a pool object implicitly frees all objects allocated from that pool. 
    // Specifically, destroying VkCommandPool frees all VkCommandBuffer objects that 
    // were allocated from it, and destroying VkDescriptorPool frees all 
    // VkDescriptorSet objects that were allocated from it.
    VK_CHECK(qvkResetDescriptorPool(vk.device, vk.descriptor_pool, 0));
    
}
