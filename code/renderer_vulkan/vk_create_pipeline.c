#include "qvk.h"
#include "tr_local.h"
#include "vk_instance.h"
#include "vk_create_pipeline.h"
#include "vk_shaders.h"


// The graphics pipeline is the sequence of operations that take the vertices
// and textures of your meshes all the way to the pixels in the render targets
//
// The input assembler collects the raw vertex data from the buffers you specify
// and may also use an index buffer to repeat cartain elements without having to
// duplicate the vertex data itself.
//
// The vertex shader is run for every vertex and generally applies transformations
// to turn vertex positions from model space to screen space. It also passes
// per-vertex data down the pipeline
//
// The tessllation shaders allow you to subdivide geometry based on certain rules
// to increase the mesh quality. This is often make surfaces like brick walls
// and staircases look less flat when they are nearby.
//
// The geometry shader is run on every primitive(triangle, line, point) and can
// discard it or output more primitives than came in. This is similar to the
// tessellation shader, but much more flexible.
//
// The rasterization stage discretizes the primitives into fragments. These are
// the pixel elements that they fill on the framebuffer. Any fragments that fall
// outside the screen are discarded and the attributes outputted by the vertex
// shader are interpolated across the fragments.
//
// The fragment shader is invoked for every fragment that servives and determines
// which framebuffer(s) the fragment are written to and with which color and depth
// values. It can do this using the interpolated data from the vertex shader,
// which can include things like texture coordinates and normals for lighting.
//
// The color blending stage applies operations to mix different fragments that
// map to the same pixel in the framebuffer. Fragments can simply overwrite
// each other, add up or be mixed based opon transparency.
//
// 



// used with cg_shadows == 2
enum Vk_Shadow_Phase {
    shadows_disabled,
	shadow_edges_rendering,
	fullscreen_quad_rendering
};


struct Vk_Pipeline_Def {
	uint32_t state_bits; // GLS_XXX flags
	int face_culling;// cullType_t
	VkBool32 polygon_offset;
	VkBool32 clipping_plane;
	VkBool32 mirror;
	VkBool32 line_primitives;
    enum Vk_Shader_Type shader_type;
	enum Vk_Shadow_Phase shadow_phase;
};



static int s_numPipelines = 0;

#define MAX_VK_PIPELINES        1024
static struct Vk_Pipeline_Def s_pipeline_defs[MAX_VK_PIPELINES];
static VkPipeline s_pipelines[MAX_VK_PIPELINES];


void R_PipelineList_f(void)
{
    ri.Printf(PRINT_ALL, " Total pipeline created: %d\n", s_numPipelines);
}


static void vk_create_pipeline(const struct Vk_Pipeline_Def* def, VkPipeline* pPipeLine)
{


    // Two stages: vs and fs
    VkPipelineShaderStageCreateInfo shaderStages[2];

    vk_createShaderStages(def->state_bits, def->shader_type, def->clipping_plane, shaderStages);

	//
	// Vertex input
	//
	VkVertexInputBindingDescription bindings[4];
	// xyz array
	bindings[0].binding = 0;
	bindings[0].stride = sizeof(vec4_t);
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// color array
	bindings[1].binding = 1;
	bindings[1].stride = sizeof(color4ub_t);
	bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// st0 array
	bindings[2].binding = 2;
	bindings[2].stride = sizeof(vec2_t);
	bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// st1 array
	bindings[3].binding = 3;
	bindings[3].stride = sizeof(vec2_t);
	bindings[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribs[4];
	// xyz
	attribs[0].location = 0;
	attribs[0].binding = 0;
	attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribs[0].offset = 0;

	// color
	attribs[1].location = 1;
	attribs[1].binding = 1;
	attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attribs[1].offset = 0;

	// st0
	attribs[2].location = 2;
	attribs[2].binding = 2;
	attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[2].offset = 0;

	// st1
	attribs[3].location = 3;
	attribs[3].binding = 3;
	attribs[3].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[3].offset = 0;

	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = NULL;
	vertex_input_state.flags = 0;
	vertex_input_state.vertexBindingDescriptionCount = (def->shader_type == single_texture) ? 3 : 4;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.vertexAttributeDescriptionCount = (def->shader_type == single_texture) ? 3 : 4;
	vertex_input_state.pVertexAttributeDescriptions = attribs;

	//
	// Primitive assembly.
	//
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = def->line_primitives ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	//
	// Viewport.
	//
	VkPipelineViewportStateCreateInfo viewport_state;
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL; // dynamic scissor state

	//
	// Rasterization.
	// The rasterizer takes the geometry that is shaped by the vertices
    // from the vertex shader and turns it into fragments to be colored
    // by the fragment shader. It also performs depth testing, face culling
    // and the scissor test.
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = NULL;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;

    // how fragments are generated for geometry.
	rasterization_state.polygonMode = (def->state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

	if (def->face_culling == CT_TWO_SIDED)
		rasterization_state.cullMode = VK_CULL_MODE_NONE;
	else if (def->face_culling == CT_FRONT_SIDED)
		rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
	else if (def->face_culling == CT_BACK_SIDED)
		rasterization_state.cullMode = (def->mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
	else
		ri.Error(ERR_DROP, "create_pipeline: invalid face culling mode\n");

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	rasterization_state.depthBiasEnable = def->polygon_offset ? VK_TRUE : VK_FALSE;
	rasterization_state.depthBiasConstantFactor = 0.0f; // dynamic depth bias state
	rasterization_state.depthBiasClamp = 0.0f; // dynamic depth bias state
	rasterization_state.depthBiasSlopeFactor = 0.0f; // dynamic depth bias state
	rasterization_state.lineWidth = 1.0f;



	VkPipelineMultisampleStateCreateInfo multisample_state;
	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = NULL;
	multisample_state.flags = 0;
	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = NULL;
	multisample_state.alphaToCoverageEnable = VK_FALSE;
	multisample_state.alphaToOneEnable = VK_FALSE;

    // If you are using a depth and/or stencil buffer, then you also need to configure
    // the depth and stencil tests.
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = NULL;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (def->state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (def->state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
	depth_stencil_state.depthCompareOp = (def->state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def->shadow_phase != shadows_disabled) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == shadow_edges_rendering)
    {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def->face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

    }
    else if (def->shadow_phase == fullscreen_quad_rendering)
    {
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;

	}
    else
    {
		memset(&depth_stencil_state.front, 0, sizeof(depth_stencil_state.front));
		memset(&depth_stencil_state.back, 0, sizeof(depth_stencil_state.back));
	}

	depth_stencil_state.minDepthBounds = 0.0;
	depth_stencil_state.maxDepthBounds = 0.0;


    //After a fragment shader has returned a color, it needs to be combined
    //with the color that is already in the framebuffer. This transformation
    //is known as color blending and there are two ways to do it
    //
    // 1) Mix the old and new value to produce a final color.
    // 2) combine the old and the new value using a bitwise operation.

    // contains the configuraturation per attached framebuffer
	VkPipelineColorBlendAttachmentState attachment_blend_state = {};

	attachment_blend_state.blendEnable = (def->state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def->shadow_phase == shadow_edges_rendering)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable)
    {
		switch (def->state_bits & GLS_SRCBLEND_BITS)
        {
			case GLS_SRCBLEND_ZERO:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid src blend state bits\n" );
				break;
		}
		switch (def->state_bits & GLS_DSTBLEND_BITS)
        {
			case GLS_DSTBLEND_ZERO:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "create_pipeline: invalid dst blend state bits\n" );
				break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
	}

    // Contains the global color blending settings
	VkPipelineColorBlendStateCreateInfo blend_state;
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;
    
    // A limited amount of the state that we've specified in the previous
    // structs can actually be changed without recreating the pipeline.
    // Examples are the size of the viewport, line width and blend constants
    // If we want to do that, we have to fill in a VkPipelineDynamicStateCreateInfo
    // structure like this.
	VkPipelineDynamicStateCreateInfo dynamic_state;
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = 3;
	VkDynamicState dynamic_state_array[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
	dynamic_state.pDynamicStates = dynamic_state_array;

	VkGraphicsPipelineCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.stageCount = 2;
	create_info.pStages = shaderStages;
	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = NULL;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = vk.pipeline_layout;
	create_info.renderPass = vk.render_pass;
	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK(qvkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, NULL, pPipeLine));
}


static VkPipeline vk_find_pipeline(const struct Vk_Pipeline_Def* def)
{
    int i = 0;
	for (i = 0; i < s_numPipelines; i++)
    {
		if (s_pipeline_defs[i].shader_type == def->shader_type &&
			s_pipeline_defs[i].state_bits == def->state_bits &&
			s_pipeline_defs[i].face_culling == def->face_culling &&
			s_pipeline_defs[i].polygon_offset == def->polygon_offset &&
			s_pipeline_defs[i].clipping_plane == def->clipping_plane &&
			s_pipeline_defs[i].mirror == def->mirror &&
			s_pipeline_defs[i].line_primitives == def->line_primitives &&
			s_pipeline_defs[i].shadow_phase == def->shadow_phase)
		{
			return s_pipelines[i];
		}
	}


	VkPipeline pipeline;
    vk_create_pipeline(def, &pipeline);


	s_pipeline_defs[s_numPipelines] = *def;
	s_pipelines[s_numPipelines] = pipeline;
	s_numPipelines++;
    if (s_numPipelines >= MAX_VK_PIPELINES)
    {
		ri.Error(ERR_DROP, "vk_create_pipeline: MAX_VK_PIPELINES hit\n");
	}
	return pipeline;
}



void create_pipelines_for_each_stage(shaderStage_t *pStage, shader_t* pShader)
{

    struct Vk_Pipeline_Def def;

    def.line_primitives = 0;
    def.shadow_phase = 0;
    def.face_culling = pShader->cullType;
    def.polygon_offset = (pShader->polygonOffset == qtrue);
    def.state_bits = pStage->stateBits;

    if (pStage->bundle[1].image[0] == NULL)
        def.shader_type = single_texture;
    else if (pShader->multitextureEnv == GL_MODULATE)
        def.shader_type = multi_texture_mul;
    else if (pShader->multitextureEnv == GL_ADD)
        def.shader_type = multi_texture_add;
    else
        ri.Error(ERR_FATAL, "Vulkan: could not create pipelines for q3 shader '%s'\n", pShader->name);

    def.clipping_plane = 0;
    def.mirror = 0;
    pStage->vk_pipeline = vk_find_pipeline(&def);


    def.clipping_plane = 1;
    def.mirror = 0;
    pStage->vk_portal_pipeline = vk_find_pipeline(&def);


    def.clipping_plane = 1;
    def.mirror = 1;
    pStage->vk_mirror_pipeline = vk_find_pipeline(&def);


}



void create_standard_pipelines(void)
{
    
    
    ri.Printf(PRINT_ALL, " Create skybox pipeline \n");
    {

        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.shader_type = single_texture;
        def.state_bits = 0;
        def.face_culling = CT_FRONT_SIDED;
        def.polygon_offset = qfalse;
        def.clipping_plane = qfalse;
        def.mirror = qfalse;
        
        vk_create_pipeline(&def, &vk.skybox_pipeline);
    }

    ri.Printf(PRINT_ALL, " Create Q3 stencil shadows pipeline \n");
    {
        {
            struct Vk_Pipeline_Def def;
            memset(&def, 0, sizeof(def));


            def.polygon_offset = 0;
            def.state_bits = 0;
            def.shader_type = single_texture;
            def.clipping_plane = 0;
            def.shadow_phase = shadow_edges_rendering;

            cullType_t cull_types[2] = {CT_FRONT_SIDED, CT_BACK_SIDED};
            qboolean mirror_flags[2] = {0, 1};

            int i = 0; 
            int j = 0;

            for (i = 0; i < 2; i++)
            {
                def.face_culling = cull_types[i];
                for (j = 0; j < 2; j++)
                {
                    def.mirror = mirror_flags[j];
                    
                    
                    vk_create_pipeline(&def, &vk.shadow_volume_pipelines[i][j]);
                }
            }
        }

        {
            struct Vk_Pipeline_Def def;
            memset(&def, 0, sizeof(def));

            def.face_culling = CT_FRONT_SIDED;
            def.polygon_offset = qfalse;
            def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
            def.shader_type = single_texture;
            def.clipping_plane = qfalse;
            def.mirror = qfalse;
            def.shadow_phase = fullscreen_quad_rendering;
            
            vk_create_pipeline(&def, &vk.shadow_finish_pipeline);
        }
    }


    ri.Printf(PRINT_ALL, " Create fog and dlights pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));


        def.shader_type = single_texture;
        def.clipping_plane = qfalse;
        def.mirror = qfalse;

        unsigned int fog_state_bits[2] = {
            GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL,
            GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA
        };
        unsigned int dlight_state_bits[2] = {
            GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,
            GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL
        };
        qboolean polygon_offset[2] = {qfalse, qtrue};

        int i = 0, j = 0, k = 0;

        for (i = 0; i < 2; i++)
        {
            unsigned fog_state = fog_state_bits[i];
            unsigned dlight_state = dlight_state_bits[i];

            for (j = 0; j < 3; j++)
            {
                def.face_culling = j; // cullType_t value

                for ( k = 0; k < 2; k++)
                {
                    def.polygon_offset = polygon_offset[k];

                    def.state_bits = fog_state;
                    vk_create_pipeline(&def, &vk.fog_pipelines[i][j][k]);

                    def.state_bits = dlight_state;
                    vk_create_pipeline(&def, &vk.dlight_pipelines[i][j][k]);
                }
            }
        }
    }


    // debug pipelines
    ri.Printf(PRINT_ALL, " Create tris debug pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
        vk_create_pipeline(&def, &vk.tris_debug_pipeline);
    }

    
    ri.Printf(PRINT_ALL, " Create tris mirror debug pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
        def.face_culling = CT_BACK_SIDED;
        vk_create_pipeline(&def, &vk.tris_mirror_debug_pipeline);
    }

    ri.Printf(PRINT_ALL, " Create normals debug pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_DEPTHMASK_TRUE;
        def.line_primitives = qtrue;
        vk_create_pipeline(&def, &vk.normals_debug_pipeline);
    }


    ri.Printf(PRINT_ALL, " Create surface debug pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
        vk_create_pipeline(&def, &vk.surface_debug_pipeline_solid);
    }

    ri.Printf(PRINT_ALL, " Create surface debug outline pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
        def.line_primitives = qtrue;
        vk_create_pipeline(&def, &vk.surface_debug_pipeline_outline);
    }
    
    ri.Printf(PRINT_ALL, " Create images debug pipeline \n");
    {
        struct Vk_Pipeline_Def def;
        memset(&def, 0, sizeof(def));

        def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
        vk_create_pipeline(&def, &vk.images_debug_pipeline);
    }

}


void vk_destroyALLPipeline(void)
{
    qvkDeviceWaitIdle(vk.device);
    int i;
    for (i = 0; i < s_numPipelines; i++)
    {
		qvkDestroyPipeline(vk.device, s_pipelines[i], NULL);

        memset(&s_pipelines[i], 0, sizeof(VkPipeline) );

        memset(&s_pipeline_defs[i], 0, sizeof(struct Vk_Pipeline_Def));
    }
    s_numPipelines = 0;
}
