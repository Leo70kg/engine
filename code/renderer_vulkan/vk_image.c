#include "qvk.h"
#include "tr_local.h"
#include "image_sampler.h"
#include "vk_memory.h"
#include "R_LightScaleTexture.h"
#include "vk_image.h"
#include "Vk_Instance.h"
//
// Memory allocations.
//
#define MAX_IMAGE_CHUNKS        16
#define IMAGE_CHUNK_SIZE        (32 * 1024 * 1024)

struct Chunk {
	VkDeviceMemory memory;
	VkDeviceSize used;
};


struct Vk_Image {
	VkImage handle;
	VkImageView view;

	// Descriptor set that contains single descriptor used to access the given image.
	// It is updated only once during image initialization.
	VkDescriptorSet descriptor_set;
};

// should be the same as MAX_DRAWIMAGES
#define MAX_VK_IMAGES   2048
static struct Vk_Image  s_vkImages[MAX_VK_IMAGES] = {0};


static struct Chunk s_ImageChunks[MAX_IMAGE_CHUNKS] = {0};
static int s_NumImageChunks = 0;



///////////////////////////////////////

// Host visible memory used to copy image data to device local memory.
static VkBuffer s_StagingBuffer;
static VkDeviceMemory s_MemStgBuf;
// pointer to mapped staging buffer
static unsigned char* s_pStgBuf = NULL;
static VkDeviceSize s_StagingBufferSize = 0;

////////////////////////////////////////


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



static void vk_update_descriptor_set( 
        VkDescriptorSet set, VkImageView image_view, 
        VkBool32 mipmap, VkBool32 repeat_texture )
{

	VkDescriptorImageInfo image_info;
	image_info.sampler = vk_find_sampler(mipmap, repeat_texture);
	image_info.imageView = image_view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet descriptor_write;
	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = set;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, NULL);
}



// outside of TR since it shouldn't be cleared during ref re-init
// the renderer front end should never modify glstate_t
//typedef struct {
	int			s_CurTextures[2];
	int			s_CurTmu;
//	int			texEnv[2];
//	int			faceCulling;
//	unsigned long	glStateBits;
//} glstate_t;




void GL_Bind( image_t *image )
{
	if ( s_CurTextures[s_CurTmu] != image->texnum )
    {
		s_CurTextures[s_CurTmu] = image->texnum;

        image->frameUsed = tr.frameCount;

		// VULKAN
		s_CurrentDescriptorSets[s_CurTmu] = s_vkImages[image->index].descriptor_set;
	}
}


void VK_TextureMode( void )
{

	//vk_set_sampler(3);
	int i = 0;
	// change all the existing mipmap texture objects
	for ( i = 0 ; i < tr.numImages ; i++ )
	{
		image_t* glt = tr.images[ i ];
		if ( glt->mipmap )
		{
			GL_Bind (glt);
		}
	}

    // VULKAN

    VK_CHECK(qvkDeviceWaitIdle(vk.device));
    for ( i = 0; i < tr.numImages; i++ )
    {
        if (tr.images[i]->mipmap)
        {
            vk_update_descriptor_set(s_vkImages[i].descriptor_set, s_vkImages[i].view, 1, tr.images[i]->wrapClampMode == GL_REPEAT);
        }
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
	long mask = ~(memory_requirements.alignment - 1);

	for (i = 0; i < s_NumImageChunks; i++)
 	{
		// ensure that memory region has proper alignment
		VkDeviceSize offset = (s_ImageChunks[i].used + memory_requirements.alignment - 1) & mask;

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
		alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceMemory memory;
		VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));

		chunk = &s_ImageChunks[s_NumImageChunks];
		s_NumImageChunks++;
		chunk->memory = memory;
		chunk->used = memory_requirements.size;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}


static struct Vk_Image vk_create_image(int width, int height, VkFormat format, int mip_levels, VkBool32 repeat_texture)
{
	struct Vk_Image image;
    ri.Printf(PRINT_DEVELOPER, "create image view\n");
	// create image
	{
		VkImageCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &image.handle));
		allocate_and_bind_image_memory(image.handle);
	}


	// create image view
	{
		VkImageViewCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image.handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;
		VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &image.view));
	}

	// create associated descriptor set
	{
		VkDescriptorSetAllocateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout;
		VK_CHECK(qvkAllocateDescriptorSets(vk.device, &desc, &image.descriptor_set));

		vk_update_descriptor_set(image.descriptor_set, image.view, mip_levels > 1, repeat_texture);
		s_CurrentDescriptorSets[s_CurTmu] = image.descriptor_set;
	}

	return image;
}



static void ensure_staging_buffer_allocation(VkDeviceSize size, const unsigned char* pPix)
{
    if(s_StagingBufferSize < size)
    {
        s_StagingBufferSize = size;

        if (s_StagingBuffer != VK_NULL_HANDLE)
        {
            qvkDestroyBuffer(vk.device, s_StagingBuffer, NULL);
            memset(&s_StagingBuffer, 0, sizeof(VkBuffer));
        }

        if (s_MemStgBuf != VK_NULL_HANDLE)
        {
            qvkFreeMemory(vk.device, s_MemStgBuf, NULL);
            memset(&s_MemStgBuf, 0, sizeof(VkDeviceMemory));
        }


        VkBufferCreateInfo buffer_desc;
        buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_desc.pNext = NULL;
        buffer_desc.flags = 0;
        buffer_desc.size = size;
        buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_desc.queueFamilyIndexCount = 0;
        buffer_desc.pQueueFamilyIndices = NULL;
        VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &s_StagingBuffer));

        // To determine the memory requirements for a buffer resource

        VkMemoryRequirements memory_requirements;
        qvkGetBufferMemoryRequirements(vk.device, s_StagingBuffer, &memory_requirements);

        uint32_t memory_type = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = memory_type;
        VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &s_MemStgBuf));
        VK_CHECK(qvkBindBufferMemory(vk.device, s_StagingBuffer, s_MemStgBuf, 0));

        void* data;
        VK_CHECK(qvkMapMemory(vk.device, s_MemStgBuf, 0, VK_WHOLE_SIZE, 0, &data));

        s_pStgBuf = (unsigned char *)data;
    }

    memcpy(s_pStgBuf, pPix, size);
}


static void vk_upload_image_data(VkImage image, int width, int height, 
        qboolean mipmap, const unsigned char* pixels, int bytes_per_pixel)
{

	VkBufferImageCopy regions[16];
	int num_regions = 0;

	int buffer_size = 0;

	while (1)
    {
		VkBufferImageCopy region;
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
        region.imageOffset.y = 0;
		region.imageOffset.z = 0;

        region.imageExtent.width = width;
		region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        
		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * bytes_per_pixel;

		if (!mipmap || (width == 1 && height == 1))
			break;

		width >>= 1;
		if (width < 1)
            width = 1;

		height >>= 1;
		if (height < 1)
            height = 1;
	}

	ensure_staging_buffer_allocation(buffer_size, pixels);


	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;


	VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));
	
    //// recorder(command_buffer);
    //// recorder = [&image, &num_regions, &regions](VkCommandBuffer command_buffer)
    {
        VkCommandBuffer cb = command_buffer;
            record_buffer_memory_barrier(cb, s_StagingBuffer,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

	    record_image_layout_transition(cb, image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	    qvkCmdCopyBufferToImage(cb, s_StagingBuffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);

	    record_image_layout_transition(cb, image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    ////////
    
    VK_CHECK(qvkEndCommandBuffer(command_buffer));

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VkResult res = qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE);	
	if (res < 0) 
		ri.Error(ERR_FATAL,
        "vk_upload_image_data: error code %d returned by qvkQueueSubmit", res);


    VK_CHECK(qvkQueueWaitIdle(vk.queue));
	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);
}

/*
// VULKAN
static struct Vk_Image upload_vk_image(const struct Image_Upload_Data* upload_data, qboolean isRepTex)
{
	int w = upload_data->base_level_width;
	int h = upload_data->base_level_height;


	unsigned char* buffer = upload_data->buffer;
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	int bytes_per_pixel = 4;

	if (r_texturebits->integer <= 16)
    {

    	qboolean has_alpha = qfalse;
	    for (int i = 0; i < w * h; i++) {
		if (upload_data->buffer[i*4 + 3] != 255)  {
			has_alpha = qtrue;
			break;
		}
	    }

		buffer = (byte*) ri.Hunk_AllocateTempMemory( upload_data->buffer_size / 2 );
		format = has_alpha ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		bytes_per_pixel = 2;
	}
	if (format == VK_FORMAT_A1R5G5B5_UNORM_PACK16) {
		uint16_t* p = (uint16_t*)buffer;
		for (int i = 0; i < upload_data->buffer_size; i += 4, p++) {
			byte r = upload_data->buffer[i+0];
			byte g = upload_data->buffer[i+1];
			byte b = upload_data->buffer[i+2];

			*p = (uint32_t)((b/255.0) * 31.0 + 0.5) |
				((uint32_t)((g/255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r/255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
	} else if (format == VK_FORMAT_B4G4R4A4_UNORM_PACK16) {
		uint16_t* p = (uint16_t*)buffer;
		for (int i = 0; i < upload_data->buffer_size; i += 4, p++) {
			byte r = upload_data->buffer[i+0];
			byte g = upload_data->buffer[i+1];
			byte b = upload_data->buffer[i+2];
			byte a = upload_data->buffer[i+3];

			*p = (uint32_t)((a/255.0) * 15.0 + 0.5) |
				((uint32_t)((r/255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g/255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b/255.0) * 15.0 + 0.5) << 12);
		}
	}


	struct Vk_Image image = vk_create_image(w, h, format, upload_data->mip_levels, isRepTex);
	vk_upload_image_data(image.handle, w, h, upload_data->mip_levels > 1, buffer, bytes_per_pixel);

	if (bytes_per_pixel == 2)
		ri.Hunk_FreeTempMemory(buffer);

	return image;
}
*/

void qDestroyImage(void)
{
    vk_free_sampler();

	vk_free_staging_buffer();   

	vk_free_chunk();


	int i = 0;
	for (i = 0; i < MAX_VK_IMAGES; i++)
    {
		struct Vk_Image* image = &s_vkImages[i];

		if (image->handle != VK_NULL_HANDLE)
        {
			qvkDestroyImage(vk.device, image->handle, NULL);
			qvkDestroyImageView(vk.device, image->view, NULL);
            image->handle = VK_NULL_HANDLE;
		}

        if(image->descriptor_set != VK_NULL_HANDLE)
        {    
            qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &image->descriptor_set);
            image->descriptor_set = VK_NULL_HANDLE;
        }
	}
    
    memset(s_vkImages, 0, MAX_VK_IMAGES*sizeof(struct Vk_Image));
	
    memset(s_CurrentDescriptorSets, 0,  2 * sizeof(VkDescriptorSet));

    R_resetGammaIntensityTable();

    ///////////////////////////////////
    s_CurTextures[0] = s_CurTextures[1] = 0;
	s_CurTmu = 0;
}



#define FILE_HASH_SIZE	1024
static	image_t*		hashTable[FILE_HASH_SIZE];


static int generateHashValue( const char *fname )
{
	int		i = 0;
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



/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, unsigned char* pic, int width, int height,
						qboolean mipmap, qboolean allowPicmip, int glWrapClampMode )
{
	if (strlen(name) >= MAX_QPATH ) {
		ri.Error (ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name);
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n");
	}

	// Create image_t object.
	image_t* pImage = tr.images[tr.numImages] = (image_t*) ri.Hunk_Alloc( sizeof( image_t ), h_low );
    pImage->index = tr.numImages;
	pImage->texnum = 1024 + tr.numImages;
	pImage->mipmap = mipmap;
	pImage->allowPicmip = allowPicmip;
	strcpy (pImage->imgName, name);
	pImage->width = width;
	pImage->height = height;
	pImage->wrapClampMode = glWrapClampMode;

	int hash = generateHashValue(name);
	pImage->next = hashTable[hash];
	hashTable[hash] = pImage;

	tr.numImages++;

	// Create corresponding GPU resource.
	int isLightmap = (strncmp(name, "*lightmap", 9) == 0);
	s_CurTmu = isLightmap;
	
    GL_Bind(pImage);

	struct Image_Upload_Data upload_data;
    memset(&upload_data, 0, sizeof(upload_data));


    generate_image_upload_data(&upload_data, pic, width, height, mipmap, allowPicmip);


	// s_vkImages[image->index] = upload_vk_image(&upload_data, glWrapClampMode == GL_REPEAT);

	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	int bytes_per_pixel = 4;
    
    s_vkImages[pImage->index] = vk_create_image(upload_data.base_level_width, 
        upload_data.base_level_height, format, upload_data.mip_levels, glWrapClampMode == GL_REPEAT);
	
    vk_upload_image_data(s_vkImages[pImage->index].handle, upload_data.base_level_width, 
        upload_data.base_level_height, upload_data.mip_levels > 1, upload_data.buffer, bytes_per_pixel);


	if (isLightmap) {
		s_CurTmu = 0;
	}
	ri.Hunk_FreeTempMemory(upload_data.buffer);
	return pImage;
}



image_t* R_FindImageFile(const char *name, qboolean mipmap, 
						qboolean allowPicmip, int glWrapClampMode)
{

   	image_t* image;
	int	width, height;
	unsigned char* pic;

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
    R_LoadImage2( name, &pic, &width, &height );
	if (pic == NULL)
	{
        return NULL;
    }

	image = R_CreateImage( name, pic, width, height,
							mipmap, allowPicmip, glWrapClampMode );
	ri.Free( pic );
	return image;
}


void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
    //ri.Printf(PRINT_ALL, "w=%d, h=%d, cols=%d, rows=%d, client=%d, dirty=%d\n", 
    //       w, h, cols, rows, client, dirty);
	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
    if ( cols != tr.scratchImage[client]->width || 
            rows != tr.scratchImage[client]->height )
    {
        tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
        tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;

        // VULKAN
        struct Vk_Image* pImage = &s_vkImages[tr.scratchImage[client]->index];
        qvkDestroyImage(vk.device, pImage->handle, NULL);
        qvkDestroyImageView(vk.device, pImage->view, NULL);
        qvkFreeDescriptorSets(vk.device, vk.descriptor_pool, 1, &pImage->descriptor_set);
        s_vkImages[tr.scratchImage[client]->index] = vk_create_image(cols, rows, VK_FORMAT_R8G8B8A8_UNORM, 1, 0);
        vk_upload_image_data(pImage->handle, cols, rows, 0, data, 4);
    }
    else if (dirty)
    {
        // otherwise, just subimage upload it so that
        // drivers can tell we are going to be changing
        // it and don't try and do a texture compression       
        vk_upload_image_data(s_vkImages[tr.scratchImage[client]->index].handle, cols, rows, 0, data, 4);
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

