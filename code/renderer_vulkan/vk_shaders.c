#include "qvk.h"
#include "tr_local.h"


#include "vk_instance.h"




static VkShaderModule create_shader_module(const unsigned char* pBytes, int count)
{
	if (count % 4 != 0) {
		ri.Error(ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not multiple of 4");
	}
	VkShaderModuleCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)pBytes;
			   
	VkShaderModule module;
	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));
	return module;
};


void vk_createShaderModules(void)
{

    extern unsigned char single_texture_vert_spv[];
    extern long long single_texture_vert_spv_size;
    vk.single_texture_vs = create_shader_module(single_texture_vert_spv, single_texture_vert_spv_size);

    extern unsigned char single_texture_clipping_plane_vert_spv[];
    extern long long single_texture_clipping_plane_vert_spv_size;
    vk.single_texture_clipping_plane_vs = create_shader_module(single_texture_clipping_plane_vert_spv, single_texture_clipping_plane_vert_spv_size);

    extern unsigned char single_texture_frag_spv[];
    extern long long single_texture_frag_spv_size;
    vk.single_texture_fs = create_shader_module(single_texture_frag_spv, single_texture_frag_spv_size);

    extern unsigned char multi_texture_vert_spv[];
    extern long long multi_texture_vert_spv_size;
    vk.multi_texture_vs = create_shader_module(multi_texture_vert_spv, multi_texture_vert_spv_size);

    extern unsigned char multi_texture_clipping_plane_vert_spv[];
    extern long long multi_texture_clipping_plane_vert_spv_size;
    vk.multi_texture_clipping_plane_vs = create_shader_module(multi_texture_clipping_plane_vert_spv, multi_texture_clipping_plane_vert_spv_size);

    extern unsigned char multi_texture_mul_frag_spv[];
    extern long long multi_texture_mul_frag_spv_size;
    vk.multi_texture_mul_fs = create_shader_module(multi_texture_mul_frag_spv, multi_texture_mul_frag_spv_size);

    extern unsigned char multi_texture_add_frag_spv[];
    extern long long multi_texture_add_frag_spv_size;
    vk.multi_texture_add_fs = create_shader_module(multi_texture_add_frag_spv, multi_texture_add_frag_spv_size);
}


void vk_destroyShaderModules(void)
{
	qvkDestroyShaderModule(vk.device, vk.single_texture_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.single_texture_clipping_plane_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.single_texture_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_clipping_plane_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_mul_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_add_fs, NULL);
}
