#include "qvk.h"


#include "vk_instance.h"
#include "vk_shaders.h"

// Vulkan has to be specified in a bytecode format which is called SPIR-V
// and is designed to be work with both Vulkan and OpenCL.
//
// The graphics pipeline is the sequence of the operations that take the
// vertices and textures of your meshes all way to the pixels in the the
// render targets.


/*
static VkPipelineShaderStageCreateInfo get_shader_stage_desc(
        VkShaderStageFlagBits stage, VkShaderModule shader_module, const char* entry)
{
	VkPipelineShaderStageCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.stage = stage;
	desc.module = shader_module;
	desc.pName = entry;
	desc.pSpecializationInfo = NULL;
	return desc;
};
*/


// The function will take a buffer with the bytecode and the size of the buffer as parameter
// and craete VkShaderModule from it

static void create_shader_module(const unsigned char* pBytes, const int count, VkShaderModule* pVkShaderMod)
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
			   
	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, pVkShaderMod));
}



// The VkShaderModule object is just a dumb wrapper around the bytecode buffer
// The shaders aren't linked to each other yet and they haven't even been given
// a purpose yet.
void vk_createShaderModules(void)
{

    extern unsigned char single_texture_vert_spv[];
    extern int single_texture_vert_spv_size;
    
    create_shader_module(single_texture_vert_spv, single_texture_vert_spv_size,
            &vk.single_texture_vs);

    extern unsigned char single_texture_clipping_plane_vert_spv[];
    extern int single_texture_clipping_plane_vert_spv_size;
    
    create_shader_module(single_texture_clipping_plane_vert_spv, single_texture_clipping_plane_vert_spv_size,
            &vk.single_texture_clipping_plane_vs);

    extern unsigned char single_texture_frag_spv[];
    extern int single_texture_frag_spv_size;
    
    create_shader_module(single_texture_frag_spv, single_texture_frag_spv_size,
            &vk.single_texture_fs);

    extern unsigned char multi_texture_vert_spv[];
    extern int multi_texture_vert_spv_size;
    
    create_shader_module(multi_texture_vert_spv, multi_texture_vert_spv_size,
            &vk.multi_texture_vs);

    extern unsigned char multi_texture_clipping_plane_vert_spv[];
    extern int multi_texture_clipping_plane_vert_spv_size;
    create_shader_module(multi_texture_clipping_plane_vert_spv, multi_texture_clipping_plane_vert_spv_size,
            &vk.multi_texture_clipping_plane_vs);

    extern unsigned char multi_texture_mul_frag_spv[];
    extern int multi_texture_mul_frag_spv_size;
    create_shader_module(multi_texture_mul_frag_spv, multi_texture_mul_frag_spv_size,
            &vk.multi_texture_mul_fs);

    extern unsigned char multi_texture_add_frag_spv[];
    extern int multi_texture_add_frag_spv_size;
    create_shader_module(multi_texture_add_frag_spv, multi_texture_add_frag_spv_size,
            &vk.multi_texture_add_fs);
}


void vk_createShaderStages(const uint32_t state_bits, const enum Vk_Shader_Type shader_type, const VkBool32 clipping_plane, VkPipelineShaderStageCreateInfo* pShaderStages)
{

	struct Specialization_Data {
		int32_t alpha_test_func;
	} specialization_data;

	if ((state_bits & GLS_ATEST_BITS) == 0)
		specialization_data.alpha_test_func = 0;
	else if (state_bits & GLS_ATEST_GT_0)
		specialization_data.alpha_test_func = 1;
	else if (state_bits & GLS_ATEST_LT_80)
		specialization_data.alpha_test_func = 2;
	else if (state_bits & GLS_ATEST_GE_80)
		specialization_data.alpha_test_func = 3;
	else
		ri.Error(ERR_DROP, "create_pipeline: invalid alpha test state bits\n");

	VkSpecializationMapEntry specialization_entries[1];
	specialization_entries[0].constantID = 0;
	specialization_entries[0].offset = offsetof(struct Specialization_Data, alpha_test_func);
	specialization_entries[0].size = sizeof(int32_t);

	VkSpecializationInfo specialization_info;
	specialization_info.mapEntryCount = 1;
	specialization_info.pMapEntries = specialization_entries;
	specialization_info.dataSize = sizeof(struct Specialization_Data);
	specialization_info.pData = &specialization_data;



    // pShaderStages[0].pName
    // pShaderStages[0].module
    // Specify the shader module containing the shader code, and the function to invoke.
    // This means that it's possible to combine multiple fragment shaders into a single
    // shader module and use differententry  points to differnentiate between their behaviors
    // In this case we'll stick to the standard main.
    switch(shader_type)
    {
        case multi_texture_add:
        {
            pShaderStages[0].module = clipping_plane ? vk.multi_texture_clipping_plane_vs : vk.multi_texture_vs;
            pShaderStages[1].module = vk.multi_texture_add_fs;
        }break;

        case multi_texture_mul:
        {
            pShaderStages[0].module = clipping_plane ? vk.multi_texture_clipping_plane_vs : vk.multi_texture_vs;
            pShaderStages[1].module = vk.multi_texture_mul_fs;
        }break;

        case single_texture:
        {
            pShaderStages[0].module = clipping_plane ? vk.single_texture_clipping_plane_vs : vk.single_texture_vs;
            pShaderStages[1].module = vk.single_texture_fs;
        }break;
    }
    // 0 vertex shader, 1 fragment shader
    pShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    // pShaderStages[0].module = *vs_module;
    pShaderStages[0].pName = "main";
    pShaderStages[0].pNext = NULL;
    pShaderStages[0].flags = 0;
	pShaderStages[0].pSpecializationInfo = NULL;

    pShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    // pShaderStages[1].module = *fs_module;
    pShaderStages[1].pName = "main";
    pShaderStages[1].pNext = NULL;
    pShaderStages[1].flags = 0;

    // pSpecializationInfo allows you to specify values for shader constants,
    // you can use a single shader module where its behavior can be configured
    // at pipeline creation by specifying different values fot the constants
    // used in it. This is more effient than configuring the shader using 
    // variables at render time, because the compiler can do optimizations.
	pShaderStages[1].pSpecializationInfo = (state_bits & GLS_ATEST_BITS) ? &specialization_info : NULL;

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
