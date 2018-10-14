#include "qvk.h"
#include "tr_local.h"

static VkSampler vk_find_sampler(const struct Vk_Sampler_Def* def)
{
	// Look for sampler among existing samplers.
	int i;
    for (i = 0; i < vk_world.num_samplers; i++)
    {
		const struct Vk_Sampler_Def* cur_def = &vk_world.sampler_defs[i];

		if (( cur_def->repeat_texture == def->repeat_texture) &&
			( cur_def->gl_mag_filter == def->gl_mag_filter) && 
			( cur_def->gl_min_filter == def->gl_min_filter) )
		{
			return vk_world.samplers[i];
		}
	}

	// Create new sampler.
	if (vk_world.num_samplers >= MAX_VK_SAMPLERS)
    {
		ri.Error(ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n");
	}

	VkSamplerAddressMode address_mode = def->repeat_texture ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter mag_filter;
	if (def->gl_mag_filter == GL_NEAREST)
    {
		mag_filter = VK_FILTER_NEAREST;
	}
    else if(def->gl_mag_filter == GL_LINEAR)
    {
		mag_filter = VK_FILTER_LINEAR;
	}
    else
    {
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
	}

	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	qboolean max_lod_0_25 = qfalse; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	if (def->gl_min_filter == GL_NEAREST) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		max_lod_0_25 = qtrue;
	}
    else if (def->gl_min_filter == GL_LINEAR)
    {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		max_lod_0_25 = qtrue;
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
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
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

	vk_world.sampler_defs[vk_world.num_samplers] = *def;
	vk_world.samplers[vk_world.num_samplers] = sampler;
	vk_world.num_samplers++;
	return sampler;
}


void vk_update_descriptor_set(VkDescriptorSet set, VkImageView image_view, qboolean mipmap, qboolean repeat_texture)
{
	struct Vk_Sampler_Def sampler_def;
	sampler_def.repeat_texture = repeat_texture;
	if (mipmap) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
	}

	VkDescriptorImageInfo image_info;
	image_info.sampler = vk_find_sampler(&sampler_def);
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
