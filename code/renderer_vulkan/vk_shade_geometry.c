#include "vk_clear_attachments.h"
#include "vk_shade_geometry.h"
#include "vk_instance.h"
#include "tr_globals.h"
#include "tr_cvar.h"
#include "vk_image.h"
#include "../renderercommon/matrix_multiplication.h"


#define VERTEX_CHUNK_SIZE   (512 * 1024)
#define INDEX_BUFFER_SIZE   (2 * 1024 * 1024)

#define XYZ_SIZE            (4 * VERTEX_CHUNK_SIZE)
#define COLOR_SIZE          (1 * VERTEX_CHUNK_SIZE)
#define ST0_SIZE            (2 * VERTEX_CHUNK_SIZE)
#define ST1_SIZE            (2 * VERTEX_CHUNK_SIZE)

#define XYZ_OFFSET          0
#define COLOR_OFFSET        (XYZ_OFFSET + XYZ_SIZE)
#define ST0_OFFSET          (COLOR_OFFSET + COLOR_SIZE)
#define ST1_OFFSET          (ST0_OFFSET + ST0_SIZE)

struct ShadingData_t
{
    // Buffers represent linear arrays of data which are used for various purposes
    // by binding them to a graphics or compute pipeline via descriptor sets or 
    // via certain commands,  or by directly specifying them as parameters to 
    // certain commands. Buffers are represented by VkBuffer handles:
	VkBuffer vertex_buffer;
	unsigned char* vertex_buffer_ptr ; // pointer to mapped vertex buffer
	uint32_t xyz_elements;
	uint32_t color_st_elements;

	VkBuffer index_buffer;
	unsigned char* index_buffer_ptr; // pointer to mapped index buffer
	uint32_t index_buffer_offset;

	// host visible memory that holds both vertex and index data
	VkDeviceMemory geometry_buffer_memory;
};

struct ShadingData_t shadingDat;



VkBuffer vk_getIndexBuffer(void)
{
    return shadingDat.index_buffer;
}


static float s_modelview_matrix[16] QALIGN(16);

void set_modelview_matrix(const float mv[16])
{
    memcpy(s_modelview_matrix, mv, 64);
}

void get_modelview_matrix(float* mv)
{
    memcpy(mv, s_modelview_matrix, 64);
}

void reset_modelview_matrix(void)
{
    Mat4Identity(s_modelview_matrix);
}


float ProjectRadius( float r, vec3_t location, float pMatProj[16] )
{
    float pr = 0;
    float tmpVec[3];
    VectorSubtract(location, tr.viewParms.or.origin, tmpVec);
	float dist = DotProduct( tr.viewParms.or.axis[0], tmpVec );

	if ( dist > 0 )
    {
    
        // vec3_t	p;
        // p[0] = 0;
        // p[1] = r ;
        // p[2] = -dist;


        //  float projected[4];
        //	projected[0] = p[0] * pMatProj[0] + p[1] * pMatProj[4] + p[2] * pMatProj[8] + pMatProj[12];
        //  projected[1] = p[0] * pMatProj[1] - p[1] * pMatProj[5] + p[2] * pMatProj[9] + pMatProj[13];
        //	projected[2] = p[0] * pMatProj[2] + p[1] * pMatProj[6] + p[2] * pMatProj[10] + pMatProj[14];
        //  projected[3] = p[0] * pMatProj[3] + p[1] * pMatProj[7] + p[2] * pMatProj[11] + pMatProj[15];
        //  pr = projected[1] / projected[3];
        
        float p1 = - r * pMatProj[5] - dist * pMatProj[9] + pMatProj[13];
        float p3 =   r * pMatProj[7] - dist * pMatProj[11] + pMatProj[15];

        pr = p1 / p3;
        

        if ( pr > 1.0f )
            pr = 1.0f;
    }

	return pr;
}




void R_SetupProjection( float pMatProj[16] )
{
	float zFar;

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are added, 
    // because they use the projection matrix for lod calculation

	// dynamically compute far clip plane distance
	// if not rendering the world (icons, menus, etc), set a 2k far clip plane

	if ( tr.refdef.rd.rdflags & RDF_NOWORLDMODEL )
    {
		zFar = 2048.0f;
	}
    else
    {
        float o[3];

        o[0] = tr.viewParms.or.origin[0];
        o[1] = tr.viewParms.or.origin[1];
        o[2] = tr.viewParms.or.origin[2];

        float farthestCornerDistance = 0;
        uint32_t	i;

        // set far clipping planes dynamically
        for ( i = 0; i < 8; i++ )
        {
            float v[3];
     
            if ( i & 1 )
            {
                v[0] = tr.viewParms.visBounds[0][0] - o[0];
            }
            else
            {
                v[0] = tr.viewParms.visBounds[1][0] - o[0];
            }

            if ( i & 2 )
            {
                v[1] = tr.viewParms.visBounds[0][1] - o[1];
            }
            else
            {
                v[1] = tr.viewParms.visBounds[1][1] - o[1];
            }

            if ( i & 4 )
            {
                v[2] = tr.viewParms.visBounds[0][2] - o[2];
            }
            else
            {
                v[2] = tr.viewParms.visBounds[1][2] - o[2];
            }

            float distance = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
            if( distance > farthestCornerDistance )
            {
                farthestCornerDistance = distance;
            }
        }
        
        zFar = tr.viewParms.zFar = sqrtf(farthestCornerDistance);
    }
	
	// set up projection matrix
	// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
    
    // Vulkan clip space has inverted Y and half Z.	
    
    float zNear	= r_znear->value;
	float p10 = -zFar / (zFar - zNear);

    float py = tan(tr.viewParms.fovY * (M_PI / 360.0f));
    float px = tan(tr.viewParms.fovX * (M_PI / 360.0f));


	pMatProj[0] = 1.0f / px;
	pMatProj[1] = 0;
	pMatProj[2] = 0;
	pMatProj[3] = 0;
    
    pMatProj[4] = 0;
	pMatProj[5] = -1.0f / py;
	pMatProj[6] = 0;
	pMatProj[7] = 0;

    pMatProj[8] = 0;	// normally 0
	pMatProj[9] =  0;
	pMatProj[10] = p10;
	pMatProj[11] = -1.0f;

    pMatProj[12] = 0;
	pMatProj[13] = 0;
	pMatProj[14] = zNear * p10;
	pMatProj[15] = 0;

}

/*
static void get_mvp_transform(float* mvp, const int isProj2D)
{
	if (isProj2D)
    {
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
	}
    else
    {
		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		
        MatrixMultiply4x4_SSE(s_modelview_matrix, backEnd.viewParms.projectionMatrix, mvp);
	}
}
*/

static VkViewport get_viewport(enum Vk_Depth_Range depth_range)
{
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
		viewport.y = backEnd.viewParms.viewportY;
		viewport.width = backEnd.viewParms.viewportWidth;
		viewport.height = backEnd.viewParms.viewportHeight;
	}

    switch(depth_range)
    {
        case normal:
        {
        	viewport.minDepth = 0.0f;
		    viewport.maxDepth = 1.0f;
        }break;

        case force_zero:
        {
		    viewport.minDepth = 0.0f;
		    viewport.maxDepth = 0.0f;
	    }break;
        
        case force_one:
        {
		    viewport.minDepth = 1.0f;
		    viewport.maxDepth = 1.0f;
	    }break;

        case weapon:
        {
            viewport.minDepth = 0.0f;
		    viewport.maxDepth = 0.3f;
        }break;
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
		//r.offset.y = glConfig.vidHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
        r.offset.y = backEnd.viewParms.viewportY;
        r.extent.width = backEnd.viewParms.viewportWidth;
		r.extent.height = backEnd.viewParms.viewportHeight;
        //ri.Printf(PRINT_ALL, "(%d, %d, %d, %d)\n", r.offset.x, r.offset.y, r.extent.width, r.extent.height);
    }

	return r;
}


// Vulkan memory is broken up into two categories, host memory and device memory.
// Host memory is memory needed by the Vulkan implementation for 
// non-device-visible storage. Allocations returned by vkAllocateMemory
// are guaranteed to meet any alignment requirement of the implementation
//
// Host access to buffer must be externally synchronized

void vk_createVertexBuffer(void)
{
    ri.Printf(PRINT_ALL, " Create vertex buffer: shadingDat.vertex_buffer \n");

    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    //VERTEX_BUFFER_SIZE
    desc.size = XYZ_SIZE + COLOR_SIZE + ST0_SIZE + ST1_SIZE;
    desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &shadingDat.vertex_buffer));
}

void vk_createIndexBuffer(void)
{
    ri.Printf(PRINT_ALL, " Create index buffer: shadingDat.index_buffer \n");

    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = NULL;
    desc.size = INDEX_BUFFER_SIZE;
    desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &shadingDat.index_buffer));
}

void vk_createGeometryBuffers(void)
{

    vk_createVertexBuffer();
    vk_createIndexBuffer();
    // The buffer has been created, but it doesn't actually have any memory
    // assigned to it yet.

    VkMemoryRequirements vb_memory_requirements;
    qvkGetBufferMemoryRequirements(vk.device, shadingDat.vertex_buffer, &vb_memory_requirements);

    VkMemoryRequirements ib_memory_requirements;
    qvkGetBufferMemoryRequirements(vk.device, shadingDat.index_buffer, &ib_memory_requirements);

    VkDeviceSize mask = (ib_memory_requirements.alignment - 1);
    VkDeviceSize idxBufOffset = ~mask & (vb_memory_requirements.size + mask);

    uint32_t memory_type_bits = vb_memory_requirements.memoryTypeBits & ib_memory_requirements.memoryTypeBits;

    VkMemoryAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.allocationSize = idxBufOffset + ib_memory_requirements.size;
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit specifies that memory allocated with
    // this type can be mapped for host access using vkMapMemory.
    //
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bit specifies that the host cache
    // management commands vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges
    // are not needed to flush host writes to the device or make device writes visible
    // to the host, respectively.
    alloc_info.memoryTypeIndex = find_memory_type(memory_type_bits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    ri.Printf(PRINT_ALL, " Allocate device memory: shadingDat.geometry_buffer_memory(%ld bytes). \n",
            alloc_info.allocationSize);

    VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &shadingDat.geometry_buffer_memory));

    // To attach memory to a buffer object
    // vk.device is the logical device that owns the buffer and memory.
    // vertex_buffer/index_buffer is the buffer to be attached to memory.
    // 
    // geometry_buffer_memory is a VkDeviceMemory object describing the 
    // device memory to attach.
    // 
    // idxBufOffset is the start offset of the region of memory 
    // which is to be bound to the buffer.
    // 
    // The number of bytes returned in the VkMemoryRequirements::size member
    // in memory, starting from memoryOffset bytes, will be bound to the 
    // specified buffer.
    qvkBindBufferMemory(vk.device, shadingDat.vertex_buffer, shadingDat.geometry_buffer_memory, 0);
    qvkBindBufferMemory(vk.device, shadingDat.index_buffer, shadingDat.geometry_buffer_memory, idxBufOffset);

    // Host Access to Device Memory Objects
    // Memory objects created with vkAllocateMemory are not directly host accessible.
    // Memory objects created with the memory property VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    // are considered mappable. 
    // Memory objects must be mappable in order to be successfully mapped on the host
    // VK_WHOLE_SIZE: map from offset to the end of the allocation.
    // &data points to a pointer in which is returned a host-accessible pointer
    // to the beginning of the mapped range. This pointer minus offset must be
    // aligned to at least VkPhysicalDeviceLimits::minMemoryMapAlignment.
    void* data;
    VK_CHECK(qvkMapMemory(vk.device, shadingDat.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
    shadingDat.vertex_buffer_ptr = (unsigned char*)data;
    shadingDat.index_buffer_ptr = (unsigned char*)data + idxBufOffset;
}


void vk_destroy_shading_data(void)
{
    ri.Printf(PRINT_ALL, " Destroy vertex/index buffer: shadingDat.vertex_buffer shadingDat.index_buffer. \n");
    ri.Printf(PRINT_ALL, " Free device memory: shadingDat.geometry_buffer_memory. \n");

    qvkDestroyBuffer(vk.device, shadingDat.vertex_buffer, NULL);
	qvkDestroyBuffer(vk.device, shadingDat.index_buffer, NULL);
    
    qvkUnmapMemory(vk.device, shadingDat.geometry_buffer_memory);
	qvkFreeMemory(vk.device, shadingDat.geometry_buffer_memory, NULL);

    memset(&shadingDat, 0, sizeof(shadingDat));
}


void vk_shade_geometry(VkPipeline pipeline, VkBool32 multitexture, enum Vk_Depth_Range depth_range, VkBool32 indexed)
{

    // color
    if ((shadingDat.color_st_elements + tess.numVertexes) * sizeof(color4ub_t) > COLOR_SIZE)
        ri.Error(ERR_DROP, "vulkan: vertex buffer overflow (color)\n");

    unsigned char* dst_color = shadingDat.vertex_buffer_ptr + COLOR_OFFSET + shadingDat.color_st_elements * sizeof(color4ub_t);
    memcpy(dst_color, tess.svars.colors, tess.numVertexes * sizeof(color4ub_t));
    // st0

    unsigned char* dst_st0 = shadingDat.vertex_buffer_ptr + ST0_OFFSET + shadingDat.color_st_elements * sizeof(vec2_t);
    memcpy(dst_st0, tess.svars.texcoords[0], tess.numVertexes * sizeof(vec2_t));

	// st1
	if (multitexture)
    {
		unsigned char* dst = shadingDat.vertex_buffer_ptr + ST1_OFFSET + shadingDat.color_st_elements * sizeof(vec2_t);
		memcpy(dst, tess.svars.texcoords[1], tess.numVertexes * sizeof(vec2_t));
	}

	// configure vertex data stream
	VkBuffer bufs[3] = { shadingDat.vertex_buffer, shadingDat.vertex_buffer, shadingDat.vertex_buffer };
	VkDeviceSize offs[3] = {
		COLOR_OFFSET + shadingDat.color_st_elements * sizeof(color4ub_t),
		ST0_OFFSET   + shadingDat.color_st_elements * sizeof(vec2_t),
		ST1_OFFSET   + shadingDat.color_st_elements * sizeof(vec2_t)
	};
    
	qvkCmdBindVertexBuffers(vk.command_buffer, 1, multitexture ? 3 : 2, bufs, offs);
	shadingDat.color_st_elements += tess.numVertexes;

	// bind descriptor sets

//    vkCmdBindDescriptorSets causes the sets numbered [firstSet.. firstSet+descriptorSetCount-1] to use
//    the bindings stored in pDescriptorSets[0..descriptorSetCount-1] for subsequent rendering commands 
//    (either compute or graphics, according to the pipelineBindPoint).
//    Any bindings that were previously applied via these sets are no longer valid.

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


void vk_resetGeometryBuffer(void)
{
	// Reset geometry buffer's current offsets.
	shadingDat.xyz_elements = 0;
	shadingDat.color_st_elements = 0;
	shadingDat.index_buffer_offset = 0;

    Mat4Identity(s_modelview_matrix);
}


void vk_bind_geometry(void) 
{

	// xyz stream
	{
		unsigned char* dst = shadingDat.vertex_buffer_ptr + XYZ_OFFSET + shadingDat.xyz_elements * sizeof(vec4_t);
		memcpy(dst, tess.xyz, tess.numVertexes * sizeof(vec4_t));

		VkDeviceSize xyz_offset = XYZ_OFFSET + shadingDat.xyz_elements * sizeof(vec4_t);
		qvkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &shadingDat.vertex_buffer, &xyz_offset);
		shadingDat.xyz_elements += tess.numVertexes;

        if (shadingDat.xyz_elements * sizeof(vec4_t) > XYZ_SIZE)
			ri.Error(ERR_DROP, "vk_bind_geometry: vertex buffer overflow (xyz)\n");
	}

	// indexes stream
	{
		size_t indexes_size = tess.numIndexes * sizeof(uint32_t);        

		unsigned char* dst = shadingDat.index_buffer_ptr + shadingDat.index_buffer_offset;
		memcpy(dst, tess.indexes, indexes_size);

		qvkCmdBindIndexBuffer(vk.command_buffer, shadingDat.index_buffer, shadingDat.index_buffer_offset, VK_INDEX_TYPE_UINT32);
		shadingDat.index_buffer_offset += indexes_size;

        if (shadingDat.index_buffer_offset > INDEX_BUFFER_SIZE)
			ri.Error(ERR_DROP, "vk_bind_geometry: index buffer overflow\n");
	}



	if (backEnd.viewParms.isPortal)
    {
        unsigned int i = 0;

        // mvp transform + eye transform + clipping plane in eye space
        float push_constants[32] QALIGN(16);
        // 32 * 4 = 128 BYTES
	    const unsigned int push_constants_size = 128;
    

        MatrixMultiply4x4_SSE(s_modelview_matrix, backEnd.viewParms.projectionMatrix, push_constants);


        //ri.Printf( PRINT_ALL, "backEnd.projection2D = %d\n", backEnd.projection2D);
		// Eye space transform.

        /*
		// NOTE: backEnd.or.modelMatrix incorporates s_flipMatrix, so it should be taken into account 
		// when computing clipping plane too.
		// Clipping plane in eye coordinates.

		float world_plane[4];
		world_plane[0] = backEnd.viewParms.portalPlane.normal[0];
		world_plane[1] = backEnd.viewParms.portalPlane.normal[1];
		world_plane[2] = backEnd.viewParms.portalPlane.normal[2];
		world_plane[3] = backEnd.viewParms.portalPlane.dist;

        float* eye_xform = push_constants + 16;
		for (i = 0; i < 12; i++)
        {
			eye_xform[i] = backEnd.or.modelMatrix[(i%4)*4 + i/4 ];
		}
        */

		push_constants[16] = backEnd.or.modelMatrix[0];
		push_constants[17] = backEnd.or.modelMatrix[4];
		push_constants[18] = backEnd.or.modelMatrix[8];
		push_constants[19] = backEnd.or.modelMatrix[12];

		push_constants[20] = backEnd.or.modelMatrix[1];
		push_constants[21] = backEnd.or.modelMatrix[5];
		push_constants[22] = backEnd.or.modelMatrix[9];
		push_constants[23] = backEnd.or.modelMatrix[13];

		push_constants[24] = backEnd.or.modelMatrix[2];
		push_constants[25] = backEnd.or.modelMatrix[6];
		push_constants[26] = backEnd.or.modelMatrix[10];
		push_constants[27] = backEnd.or.modelMatrix[14];

		float eye_plane[4];
		eye_plane[0] = DotProduct (backEnd.viewParms.or.axis[0], backEnd.viewParms.portalPlane.normal);
		eye_plane[1] = DotProduct (backEnd.viewParms.or.axis[1], backEnd.viewParms.portalPlane.normal);
		eye_plane[2] = DotProduct (backEnd.viewParms.or.axis[2], backEnd.viewParms.portalPlane.normal);
		eye_plane[3] = DotProduct (backEnd.viewParms.or.origin , backEnd.viewParms.portalPlane.normal) - backEnd.viewParms.portalPlane.dist;

		// Apply s_flipMatrix to be in the same coordinate system as eye_xfrom.
		push_constants[28] = -eye_plane[1];
		push_constants[29] =  eye_plane[2];
		push_constants[30] = -eye_plane[0];
		push_constants[31] =  eye_plane[3];


        // As described above in section Pipeline Layouts, the pipeline layout defines shader push constants
        // which are updated via Vulkan commands rather than via writes to memory or copy commands.
        // Push constants represent a high speed path to modify constant data in pipelines
        // that is expected to outperform memory-backed resource updates.
	    qvkCmdPushConstants(vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, push_constants_size, push_constants);

	}
    else
    {
      	// push constants are another way of passing dynamic values to shaders
 	    // Specify push constants.
	    float mvp[16] QALIGN(16); // mvp transform + eye transform + clipping plane in eye space
        
        //ri.Printf( PRINT_ALL, "projection2D = %d\n", backEnd.projection2D); 

        if (backEnd.projection2D)
        {
            float mvp0 = 2.0f / glConfig.vidWidth;
            float mvp5 = 2.0f / glConfig.vidHeight;

            mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
            mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
            mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 1.0f; mvp[11] = 0.0f;
            mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 0.0f; mvp[15] = 1.0f;
        }
        else
        {
            // update q3's proj matrix (opengl) to vulkan conventions:
            // z - [0, 1] instead of [-1, 1] and invert y direction

            MatrixMultiply4x4_SSE(s_modelview_matrix, backEnd.viewParms.projectionMatrix, mvp);
        }


        // As described above in section Pipeline Layouts, the pipeline layout defines shader push constants
        // which are updated via Vulkan commands rather than via writes to memory or copy commands.
        // Push constants represent a high speed path to modify constant data in pipelines
        // that is expected to outperform memory-backed resource updates.
	    qvkCmdPushConstants(vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp);
    }

}
