#include "tr_local.h"

#include "qvk.h"

/*  

#define FILE_HASH_SIZE		1024
static image_t* hashTable[FILE_HASH_SIZE] = {0};

struct Image_Upload_Data
{
	unsigned char* buffer;
	int buffer_size;
	int mip_levels;
	int base_level_width;
	int base_level_height;
};



static const unsigned char mipBlendColors[16][4] =
{
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};


static int generateHashValue( const char *fname )
{
	int		i = 0;
	long	hash = 0;

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

*/







/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
static void R_MipMap2( unsigned *in, int inWidth, int inHeight ) {
	int			i, j, k;
	unsigned char* outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (unsigned char *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
					1 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					1 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	memcpy( in, temp, outWidth * outHeight * 4 );
	ri.Hunk_FreeTempMemory( temp );
}

*/


/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
static void R_MipMap (unsigned char *in, int width, int height)
{
	int		i, j;
	unsigned char	*out;
	int		row;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)in, width, height );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}
*/




/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================

static void R_BlendOverTexture( unsigned char *data, int pixelCount, unsigned char blend[4] ) {
	int		i;
	int		inverseAlpha;
	int		premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}




static struct Image_Upload_Data generate_image_upload_data(
        const unsigned char* data,
        int width, int height, qboolean mipmap, qboolean picmip )
{
	//
	// convert to exact power of 2 sizes
	//
	int scaled_width = 1, scaled_height = 1;
    while(scaled_width < width)
        scaled_width<<=1;
    
    while(scaled_height < height)
        scaled_height<<=1;

    if ( r_roundImagesDown->integer && scaled_width > width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > height )
		scaled_height >>= 1;



	struct Image_Upload_Data upload_data;
	upload_data.buffer = (unsigned char *) ri.Hunk_AllocateTempMemory(2 * 4 * scaled_width * scaled_height);


	ri.Printf( PRINT_ALL, "generate_image_upload_data: Allocate %d byte Memory \n", 2 * 4 * scaled_width * scaled_height );



	unsigned char* resampled_buffer = NULL;
	if ( scaled_width != width || scaled_height != height )
    {
		resampled_buffer = (unsigned char *) ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture ((unsigned*)data, width, height, (unsigned*)resampled_buffer, scaled_width, scaled_height);
		data = resampled_buffer;
		width = scaled_width;
		height = scaled_height;
	}

	//
	// perform optional picmip operation
	//
	if ( picmip ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	int max_texture_size = 2048;
	while ( scaled_width > max_texture_size	|| scaled_height > max_texture_size )
	{
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	upload_data.base_level_width = scaled_width;
	upload_data.base_level_height = scaled_height;

	if (scaled_width == width && scaled_height == height && !mipmap)
    {
		upload_data.mip_levels = 1;
		upload_data.buffer_size = scaled_width * scaled_height * 4;
		
        memcpy(upload_data.buffer, data, upload_data.buffer_size);

	    ri.Printf( PRINT_ALL, "gupload_data.buffer_size:  %d \n", upload_data.buffer_size );
		if (resampled_buffer != NULL)
			ri.Hunk_FreeTempMemory(resampled_buffer);
		return upload_data;
	}

	// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
	while (width > scaled_width || height > scaled_height)
    {
		R_MipMap((unsigned char *)data, width, height);

		width >>= 1;
		if (width < 1)
            width = 1;

		height >>= 1;
		if (height < 1)
            height = 1; 
	}

	// At this point width == scaled_width and height == scaled_height.

    {
        unsigned char* scaled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( sizeof( unsigned int ) * scaled_width * scaled_height );
        
        ri.Printf( PRINT_ALL, "scaled_buffer: Allocate %d byte Memory \n", sizeof( unsigned int ) * scaled_width * scaled_height );


        memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);
        
        R_LightScaleTexture(scaled_buffer, scaled_width, scaled_height, !mipmap);

        int miplevel = 0;
        int mip_level_size = scaled_width * scaled_height * 4;

        memcpy(upload_data.buffer, scaled_buffer, mip_level_size);
        upload_data.buffer_size = mip_level_size;

        if (mipmap)
        {
            while (scaled_width > 1 || scaled_height > 1) {
                R_MipMap(scaled_buffer, scaled_width, scaled_height);

                scaled_width >>= 1;
                if (scaled_width < 1) scaled_width = 1;

                scaled_height >>= 1;
                if (scaled_height < 1) scaled_height = 1;

                miplevel++;
                mip_level_size = scaled_width * scaled_height * 4;

                if ( r_colorMipLevels->integer ) {
                    R_BlendOverTexture( scaled_buffer, scaled_width * scaled_height, mipBlendColors[miplevel] );
                }

                memcpy(upload_data.buffer + upload_data.buffer_size, scaled_buffer, mip_level_size);
                upload_data.buffer_size += mip_level_size;
            }
        }
        upload_data.mip_levels = miplevel + 1;

        ri.Hunk_FreeTempMemory(scaled_buffer);

        ri.Printf( PRINT_ALL, "Freed \n");

    }


    if (resampled_buffer != NULL)
		ri.Hunk_FreeTempMemory(resampled_buffer);

    return upload_data;
}
*/

static void allocate_and_bind_image_memory(VkImage image)
{
	VkMemoryRequirements memory_requirements;
	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if (memory_requirements.size > IMAGE_CHUNK_SIZE) {
		ri.Error(ERR_FATAL, "Vulkan: could not allocate memory, image is too large.");
	}

	struct Chunk* chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	long mask = ~(memory_requirements.alignment - 1);
	for (int i = 0; i < vk_world.num_image_chunks; i++) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = (vk_world.image_chunks[i].used + memory_requirements.alignment - 1) & mask;

		if (offset + memory_requirements.size <= IMAGE_CHUNK_SIZE) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached");
		}

		VkMemoryAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = IMAGE_CHUNK_SIZE;
		alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceMemory memory;
		VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		vk_world.num_image_chunks++;
		chunk->memory = memory;
		chunk->used = memory_requirements.size;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}



struct Vk_Image vk_create_image(int width, int height, VkFormat format, int mip_levels, qboolean repeat_texture)
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
		vk_world.current_descriptor_sets[glState.currenttmu] = image.descriptor_set;
	}

;

	return image;
}



// VULKAN
struct Vk_Image upload_vk_image(const struct Image_Upload_Data* upload_data, qboolean repeat_texture)
{
	int w = upload_data->base_level_width;
	int h = upload_data->base_level_height;


	byte* buffer = upload_data->buffer;
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	int bytes_per_pixel = 4;
/*
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
*/
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

	struct Vk_Image image = vk_create_image(w, h, format, upload_data->mip_levels, repeat_texture);
	vk_upload_image_data(image.handle, w, h, upload_data->mip_levels > 1, buffer, bytes_per_pixel);

	if (bytes_per_pixel == 2)
		ri.Hunk_FreeTempMemory(buffer);

	return image;
}
