#include "qvk.h"
#include "tr_local.h"


static VkRect2D get_viewport_rect(void)
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
	}
	return r;
}


static VkViewport get_viewport(enum Vk_Depth_Range depth_range)
{
	VkRect2D r = get_viewport_rect();

	VkViewport viewport;
	viewport.x = (float)r.offset.x;
	viewport.y = (float)r.offset.y;
	viewport.width = (float)r.extent.width;
	viewport.height = (float)r.extent.height;

	if (depth_range == force_zero) {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.0f;
	} else if (depth_range == force_one) {
		viewport.minDepth = 1.0f;
		viewport.maxDepth = 1.0f;
	} else if (depth_range == weapon) {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 0.3f;
	} else {
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}
	return viewport;
}

VkRect2D get_scissor_rect(void)
{
	VkRect2D r = get_viewport_rect();

	if (r.offset.x < 0)
		r.offset.x = 0;
	if (r.offset.y < 0)
		r.offset.y = 0;

	if (r.offset.x + r.extent.width > glConfig.vidWidth)
		r.extent.width = glConfig.vidWidth - r.offset.x;
	if (r.offset.y + r.extent.height > glConfig.vidHeight)
		r.extent.height = glConfig.vidHeight - r.offset.y;

	return r;
}


void vk_shade_geometry(VkPipeline pipeline, qboolean multitexture, enum Vk_Depth_Range depth_range, qboolean indexed)
{
	// color
	{
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(color4ub_t) > COLOR_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (color)\n");

		byte* dst = vk.vertex_buffer_ptr + COLOR_OFFSET + vk.color_st_elements * sizeof(color4ub_t);
		memcpy(dst, tess.svars.colors, tess.numVertexes * sizeof(color4ub_t));
	}
	// st0
	{
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(vec2_t) > ST0_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (st0)\n");

		byte* dst = vk.vertex_buffer_ptr + ST0_OFFSET + vk.color_st_elements * sizeof(vec2_t);
		memcpy(dst, tess.svars.texcoords[0], tess.numVertexes * sizeof(vec2_t));
	}
	// st1
	if (multitexture) {
		if ((vk.color_st_elements + tess.numVertexes) * sizeof(vec2_t) > ST1_SIZE)
			ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (st1)\n");

		byte* dst = vk.vertex_buffer_ptr + ST1_OFFSET + vk.color_st_elements * sizeof(vec2_t);
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
    vk_bind_descriptor_sets( multitexture ? 2 : 1 );

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

	vk_world.dirty_depth_attachment = qtrue;
}
