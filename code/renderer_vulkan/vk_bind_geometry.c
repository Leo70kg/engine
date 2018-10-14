#include "qvk.h"
#include "tr_local.h"


static void get_mvp_transform(float* mvp)
{
	if (backEnd.projection2D) {
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;

	} else {
		const float* p = backEnd.viewParms.projectionMatrix;

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		float zNear	= r_znear->value;
		float zFar = backEnd.viewParms.zFar;
		float P10 = -zFar / (zFar - zNear);
		float P14 = -zFar*zNear / (zFar - zNear);
		float P5 = -p[5];

		float proj[16] = {
			p[0],  p[1],  p[2], p[3],
			p[4],  P5,    p[6], p[7],
			p[8],  p[9],  P10,  p[11],
			p[12], p[13], P14,  p[15]
		};

		myGlMultMatrix(vk_world.modelview_transform, proj, mvp);
	}
}


void vk_bind_geometry(void) 
{
	// xyz stream
	{
		if ((vk.xyz_elements + tess.numVertexes) * sizeof(vec4_t) > XYZ_SIZE)
			ri.Error(ERR_DROP, "vk_bind_geometry: vertex buffer overflow (xyz)\n");

		byte* dst = vk.vertex_buffer_ptr + XYZ_OFFSET + vk.xyz_elements * sizeof(vec4_t);
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

		byte* dst = vk.index_buffer_ptr + vk.index_buffer_offset;
		memcpy(dst, tess.indexes, indexes_size);

		qvkCmdBindIndexBuffer(vk.command_buffer, vk.index_buffer, vk.index_buffer_offset, VK_INDEX_TYPE_UINT32);
		vk.index_buffer_offset += indexes_size;
	}

	//
	// Specify push constants.
	//
	float push_constants[16 + 12 + 4]; // mvp transform + eye transform + clipping plane in eye space

	get_mvp_transform(push_constants);
	int push_constants_size = 64;

	if (backEnd.viewParms.isPortal) {
		// Eye space transform.
		// NOTE: backEnd.or.modelMatrix incorporates s_flipMatrix, so it should be taken into account 
		// when computing clipping plane too.
		float* eye_xform = push_constants + 16;
		for (int i = 0; i < 12; i++) {
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
	qvkCmdPushConstants(vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, push_constants_size, push_constants);
}
