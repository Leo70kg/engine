#include "qvk.h"
#include "tr_local.h"
#include "vk_clear_attachments.h"

#include "vk_shade_geometry.h"
#include "Vk_Instance.h"

#include "mvp_matrix.h"
#include "vk_image.h"


static VkViewport get_viewport(enum Vk_Depth_Range depth_range)
{
	//VkRect2D r = get_viewport_rect();
	VkViewport viewport;

	if (backEnd.projection2D)
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = glConfig.vidWidth;
		viewport.height = glConfig.vidHeight;
	}
	else
	{
		viewport.x = backEnd.viewParms.viewportX;
		viewport.y = glConfig.vidHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		viewport.width = backEnd.viewParms.viewportWidth;
		viewport.height = backEnd.viewParms.viewportHeight;
	}


	if (depth_range == force_zero)
    {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.0f;
	}
    else if (depth_range == force_one)
    {
		viewport.minDepth = 1.0f;
		viewport.maxDepth = 1.0f;
	}
    else if (depth_range == weapon)
    {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.3f;
	}
    else
    {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}
	
    return viewport;
}


VkRect2D get_scissor_rect(void)
{
	VkRect2D r;
	if (backEnd.projection2D)
	{
		r.offset.x = 0.0f;
		r.offset.y = 0.0f;
		r.extent.width = glConfig.vidWidth;
		r.extent.height = glConfig.vidHeight;
	}
	else
	{
		r.offset.x = backEnd.viewParms.viewportX;
		r.offset.y = glConfig.vidHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		r.extent.width = backEnd.viewParms.viewportWidth;
		r.extent.height = backEnd.viewParms.viewportHeight;


        if (r.offset.x < 0)
            r.offset.x = 0;
        if (r.offset.y < 0)
            r.offset.y = 0;


        if (r.offset.x + r.extent.width > glConfig.vidWidth)
            r.extent.width = glConfig.vidWidth - r.offset.x;
        
        /*
        
        if (r.offset.y + r.extent.height > glConfig.vidHeight)
            r.extent.height = glConfig.vidHeight - r.offset.y;

        */
    }

	return r;
}


void vk_shade_geometry(VkPipeline pipeline, qboolean multitexture, enum Vk_Depth_Range depth_range, qboolean indexed)
{
	// color
	{
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(color4ub_t) > COLOR_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (color)\n");

		unsigned char* dst = vk.vertex_buffer_ptr + COLOR_OFFSET + vk.color_st_elements * sizeof(color4ub_t);
		memcpy(dst, tess.svars.colors, tess.numVertexes * sizeof(color4ub_t));
	}
	// st0
	{
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(vec2_t) > ST0_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (st0)\n");

		unsigned char* dst = vk.vertex_buffer_ptr + ST0_OFFSET + vk.color_st_elements * sizeof(vec2_t);
		memcpy(dst, tess.svars.texcoords[0], tess.numVertexes * sizeof(vec2_t));
	}
	// st1
	if (multitexture) {
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(vec2_t) > ST1_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (st1)\n");

		unsigned char* dst = vk.vertex_buffer_ptr + ST1_OFFSET + vk.color_st_elements * sizeof(vec2_t);
		memcpy(dst, tess.svars.texcoords[1], tess.numVertexes * sizeof(vec2_t));
	}

	// configure vertex data stream
	VkBuffer bufs[3] = { vk.vertex_buffer, vk.vertex_buffer, vk.vertex_buffer };
	VkDeviceSize offs[3] = {
		COLOR_OFFSET + vk.color_st_elements * sizeof(color4ub_t),
		ST0_OFFSET   + vk.color_st_elements * sizeof(vec2_t),
		ST1_OFFSET   + vk.color_st_elements * sizeof(vec2_t)
	};
	qvkCmdBindVertexBuffers(vk.command_buffer, 1, multitexture ? 3 : 2, bufs, offs);
	vk.color_st_elements += tess.numVertexes;

	// bind descriptor sets
	// unsigned int set_count = multitexture ? 2 : 1;
/*
    vkCmdBindDescriptorSets causes the sets numbered [firstSet.. firstSet+descriptorSetCount-1] to use
    the bindings stored in pDescriptorSets[0..descriptorSetCount-1] for subsequent rendering commands 
    (either compute or graphics, according to the pipelineBindPoint).
    Any bindings that were previously applied via these sets are no longer valid.
*/
	qvkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            vk.pipeline_layout, 0, (multitexture ? 2 : 1), getCurDescriptorSetsPtr(), 0, NULL);

    // bind pipeline
	qvkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// configure pipeline's dynamic state
	VkRect2D scissor_rect = get_scissor_rect();
	qvkCmdSetScissor(vk.command_buffer, 0, 1, &scissor_rect);

	VkViewport viewport = get_viewport(depth_range);
	qvkCmdSetViewport(vk.command_buffer, 0, 1, &viewport);

	if (tess.shader->polygonOffset) {
		qvkCmdSetDepthBias(vk.command_buffer, r_offsetUnits->value, 0.0f, r_offsetFactor->value);
	}

	// issue draw call
	if (indexed)
        qvkCmdDrawIndexed(vk.command_buffer, tess.numIndexes, 1, 0, 0, 0);
    else
		qvkCmdDraw(vk.command_buffer, tess.numVertexes, 1, 0, 0);

	set_depth_attachment(VK_TRUE);
}


void vk_bind_geometry(void) 
{
	// xyz stream
	{
		if ((vk.xyz_elements + tess.numVertexes) * sizeof(vec4_t) > XYZ_SIZE)
			ri.Error(ERR_DROP, "vk_bind_geometry: vertex buffer overflow (xyz)\n");

		unsigned char* dst = vk.vertex_buffer_ptr + XYZ_OFFSET + vk.xyz_elements * sizeof(vec4_t);
		memcpy(dst, tess.xyz, tess.numVertexes * sizeof(vec4_t));

		VkDeviceSize xyz_offset = XYZ_OFFSET + vk.xyz_elements * sizeof(vec4_t);
		qvkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vk.vertex_buffer, &xyz_offset);
		vk.xyz_elements += tess.numVertexes;
	}

	// indexes stream
	{
		size_t indexes_size = tess.numIndexes * sizeof(uint32_t);        

		if (vk.index_buffer_offset + indexes_size > INDEX_BUFFER_SIZE)
			ri.Error(ERR_DROP, "vk_bind_geometry: index buffer overflow\n");

		unsigned char* dst = vk.index_buffer_ptr + vk.index_buffer_offset;
		memcpy(dst, tess.indexes, indexes_size);

		qvkCmdBindIndexBuffer(vk.command_buffer, vk.index_buffer, vk.index_buffer_offset, VK_INDEX_TYPE_UINT32);
		vk.index_buffer_offset += indexes_size;
	}

	//
	// Specify push constants.
	// 32 * 4 = 128 BYTES
	float push_constants[16 + 12 + 4]; // mvp transform + eye transform + clipping plane in eye space

	get_mvp_transform(push_constants);
	int push_constants_size = 64;
    int i = 0 ;

	if (backEnd.viewParms.isPortal)
    {
		// Eye space transform.
		// NOTE: backEnd.or.modelMatrix incorporates s_flipMatrix, so it should be taken into account 
		// when computing clipping plane too.
		float* eye_xform = push_constants + 16;
		for (i = 0; i < 12; i++) {
			eye_xform[i] = backEnd.or.modelMatrix[(i%4)*4 + i/4 ];
		}

		// Clipping plane in eye coordinates.
		float world_plane[4];
		world_plane[0] = backEnd.viewParms.portalPlane.normal[0];
		world_plane[1] = backEnd.viewParms.portalPlane.normal[1];
		world_plane[2] = backEnd.viewParms.portalPlane.normal[2];
		world_plane[3] = backEnd.viewParms.portalPlane.dist;

		float eye_plane[4];
		eye_plane[0] = DotProduct (backEnd.viewParms.or.axis[0], world_plane);
		eye_plane[1] = DotProduct (backEnd.viewParms.or.axis[1], world_plane);
		eye_plane[2] = DotProduct (backEnd.viewParms.or.axis[2], world_plane);
		eye_plane[3] = DotProduct (world_plane, backEnd.viewParms.or.origin) - world_plane[3];

		// Apply s_flipMatrix to be in the same coordinate system as eye_xfrom.
		push_constants[28] = -eye_plane[1];
		push_constants[29] =  eye_plane[2];
		push_constants[30] = -eye_plane[0];
		push_constants[31] =  eye_plane[3];

		push_constants_size += 64;
	}

    // As described above in section Pipeline Layouts, the pipeline layout defines shader push constants
    // which are updated via Vulkan commands rather than via writes to memory or copy commands.
    // Push constants represent a high speed path to modify constant data in pipelines
    // that is expected to outperform memory-backed resource updates.
	qvkCmdPushConstants(vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, push_constants_size, push_constants);
}

// VULKAN
void RB_ShowImages(void)
{
    int i = 0;
	if ( !backEnd.projection2D )
    {
        backEnd.projection2D = qtrue;

        // set 2D virtual screen size


        // set time for 2D shaders
        backEnd.refdef.time = ri.Milliseconds();
        backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
	}

	float black[4] = {0, 0, 0, 1};

	vk_clear_attachments(qfalse, qtrue, black);

	for (i = 0 ; i < tr.numImages ; i++)
    {
		image_t* image = tr.images[i];

		float w = glConfig.vidWidth / 20;
		float h = glConfig.vidHeight / 15;
		float x = i % 20 * w;
		float y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );

		tess.numIndexes = 6;
		tess.numVertexes = 4;

		tess.indexes[0] = 0;
		tess.indexes[1] = 1;
		tess.indexes[2] = 2;
		tess.indexes[3] = 0;
		tess.indexes[4] = 2;
		tess.indexes[5] = 3;

		tess.xyz[0][0] = x;
		tess.xyz[0][1] = y;
		tess.svars.texcoords[0][0][0] = 0;
		tess.svars.texcoords[0][0][1] = 0;

		tess.xyz[1][0] = x + w;
		tess.xyz[1][1] = y;
		tess.svars.texcoords[0][1][0] = 1;
		tess.svars.texcoords[0][1][1] = 0;

		tess.xyz[2][0] = x + w;
		tess.xyz[2][1] = y + h;
		tess.svars.texcoords[0][2][0] = 1;
		tess.svars.texcoords[0][2][1] = 1;

		tess.xyz[3][0] = x;
		tess.xyz[3][1] = y + h;
		tess.svars.texcoords[0][3][0] = 0;
		tess.svars.texcoords[0][3][1] = 1;


        vk_bind_geometry();
        vk_shade_geometry(vk.images_debug_pipeline, qfalse, normal, qtrue);

	}
	tess.numIndexes = 0;
	tess.numVertexes = 0;
}

