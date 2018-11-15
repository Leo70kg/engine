
static VkSampler CreateNewSampler(const struct Vk_Sampler_Def* def)
{

	VkSamplerAddressMode address_mode = def->repeat_texture ?
        VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter mag_filter;
	if(def->gl_mag_filter == GL_NEAREST)
    {
		mag_filter = VK_FILTER_NEAREST;
	}
    else if(def->gl_mag_filter == GL_LINEAR)
    {
		mag_filter = VK_FILTER_LINEAR;
	}
    else
    {
		ri.Error(ERR_FATAL, "invalid gl_mag_filter");
	}

	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	
    //used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter    
    VkBool32 max_lod_0_25 = 0;

    if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		max_lod_0_25 = 1;
	}
    else if (def->gl_min_filter == GL_LINEAR)
    {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		max_lod_0_25 = 1;
	}
    else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST)
    {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
    else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST)
    {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
    else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR)
    {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
    else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR)
    {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
    else {
		ri.Error(ERR_FATAL, "invalid gl_min_filter");
	}

	VkSamplerCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;
	desc.anisotropyEnable = VK_FALSE;
	desc.maxAnisotropy = 1;
	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = max_lod_0_25 ? 0.25f : 12.0f;
	desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;



	VkSampler sampler;
	VK_CHECK(qvkCreateSampler(vk.device, &desc, NULL, &sampler));


	s_ImgSamplers[s_NumSamplers++] = sampler;
	if (s_NumSamplers >= MAX_VK_SAMPLERS)
    {
		ri.Error(ERR_DROP, "MAX_VK_SAMPLERS hit\n");
	}
    return sampler;
}
