
extern cvar_t	*r_texturebits;			// number of desired texture bits
										// 0 = use framebuffer depth
										// 16 = use 16-bit textures
										// 32 = use 32-bit textures
										// all else = error

/*
// VULKAN
static struct Vk_Image upload_vk_image(const struct Image_Upload_Data* upload_data, VkBool32 isRepTex)
{
	int w = upload_data->base_level_width;
	int h = upload_data->base_level_height;


	unsigned char* buffer = upload_data->buffer;
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	int bytes_per_pixel = 4;

	if (r_texturebits->integer <= 16)
    {

    	VkBool32 has_alpha = qfalse;
	    for (int i = 0; i < w * h; i++) {
		if (upload_data->buffer[i*4 + 3] != 255)  {
			has_alpha = qtrue;
			break;
		}
	    }

		buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( upload_data->buffer_size / 2 );
		format = has_alpha ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		bytes_per_pixel = 2;
	}
	if (format == VK_FORMAT_A1R5G5B5_UNORM_PACK16) {
		uint16_t* p = (uint16_t*)buffer;
		for (int i = 0; i < upload_data->buffer_size; i += 4, p++) {
			unsigned char r = upload_data->buffer[i+0];
			unsigned char g = upload_data->buffer[i+1];
			unsigned char b = upload_data->buffer[i+2];

			*p = (uint32_t)((b/255.0) * 31.0 + 0.5) |
				((uint32_t)((g/255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r/255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
	} else if (format == VK_FORMAT_B4G4R4A4_UNORM_PACK16) {
		uint16_t* p = (uint16_t*)buffer;
		for (int i = 0; i < upload_data->buffer_size; i += 4, p++) {
			unsigned char r = upload_data->buffer[i+0];
			unsigned char g = upload_data->buffer[i+1];
			unsigned char b = upload_data->buffer[i+2];
			unsigned char a = upload_data->buffer[i+3];

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
