#include "tr_globals.h"
#include "../renderercommon/ref_import.h"
#include "vk_shade_geometry.h"
#include "vk_instance.h"
#include "vk_image.h"
#include "vk_pipelines.h"
#include "tr_cvar.h"




/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever was there.
This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/

void RB_ShowImages(void)
{

    backEnd.projection2D = qtrue;

	const float black[4] = {0, 0, 0, 1};
	vk_clearColorAttachments(black);
    
    uint32_t i;
	for (i = 0 ; i < tr.numImages ; i++)
    {
		image_t* image = tr.images[i];

		float w = glConfig.vidWidth / 20;
		float h = glConfig.vidHeight / 15;
		float x = i % 20 * w;
		float y = i / 20 * h;


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
		
        
        updateCurDescriptor( image->descriptor_set, 0);
        vk_bind_geometry();
        vk_shade_geometry(g_stdPipelines.images_debug_pipeline, VK_FALSE, DEPTH_RANGE_NORMAL, VK_TRUE);

	}
	tess.numIndexes = 0;
	tess.numVertexes = 0;
    
    backEnd.projection2D = qfalse;
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
void DrawTris (shaderCommands_t *input)
{
	updateCurDescriptor( tr.whiteImage->descriptor_set, 0);

	// VULKAN

    memset(tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
    VkPipeline pipeline = backEnd.viewParms.isMirror ? g_stdPipelines.tris_mirror_debug_pipeline : g_stdPipelines.tris_debug_pipeline;
    vk_shade_geometry(pipeline, VK_FALSE, DEPTH_RANGE_ZERO, VK_FALSE);

}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
void DrawNormals (shaderCommands_t *input)
{
	// VULKAN

    vec4_t xyz[SHADER_MAX_VERTEXES];
    memcpy(xyz, tess.xyz, tess.numVertexes * sizeof(vec4_t));
    memset(tess.svars.colors, tr.identityLightByte, SHADER_MAX_VERTEXES * sizeof(color4ub_t));

    int numVertexes = tess.numVertexes;
    int i = 0;
    while (i < numVertexes)
    {
        int count = numVertexes - i;
        if (count >= SHADER_MAX_VERTEXES/2 - 1)
            count = SHADER_MAX_VERTEXES/2 - 1;

        int k;
        for (k = 0; k < count; k++)
        {
            VectorCopy(xyz[i + k], tess.xyz[2*k]);
            VectorMA(xyz[i + k], 2, input->normal[i + k], tess.xyz[2*k + 1]);
        }
        tess.numVertexes = 2 * count;
        tess.numIndexes = 0;


        vk_bind_geometry();
        vk_shade_geometry(g_stdPipelines.normals_debug_pipeline, VK_FALSE, DEPTH_RANGE_ZERO, VK_TRUE);

        i += count;
    }

}
