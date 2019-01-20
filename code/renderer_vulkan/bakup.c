
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

static void vk_create_image(uint32_t width, uint32_t height, uint32_t mipLevels, VkBool32 repeat_texture, struct Vk_Image* pImg)
{
	// create image
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

		VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &pImg->handle));
		allocate_and_bind_image_memory(pImg->handle);
	}


	// create image view
	{
		VkImageViewCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = pImg->handle;
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
		
        VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &pImg->view));
	}

	// create associated descriptor set
    // Allocate a descriptor set from the pool. 
    // Note that we have to provide the descriptor set layout that 
    // we defined in the pipeline_layout sample. 
    // This layout describes how the descriptor set is to be allocated.
	{
		VkDescriptorSetAllocateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout;
		
        VK_CHECK(qvkAllocateDescriptorSets(vk.device, &desc, &pImg->descriptor_set));
        ri.Printf(PRINT_ALL, " Allocate Descriptor Sets \n");

        VkBool32 mipmap = (mipLevels > 1) ? VK_TRUE: VK_FALSE;
        
        VkDescriptorImageInfo image_info;
        image_info.sampler = vk_find_sampler(mipmap, repeat_texture);
        image_info.imageView = pImg->view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptor_write;
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = pImg->descriptor_set;
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
        
        
        s_CurrentDescriptorSets[s_CurTmu] = pImg->descriptor_set;
	}
}



unsigned int R_SumOfUsedImages( void )
{
	unsigned int i;

	unsigned int total = 0;
	for ( i = 0; i < tr.numImages; i++ )
    {
		if ( tr.images[i]->frameUsed == tr.frameCount )
        {
			total += tr.images[i]->uploadWidth * tr.images[i]->uploadHeight;
		}
	}

	return total;
}

*/
