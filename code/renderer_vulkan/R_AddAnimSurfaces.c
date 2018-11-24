#include "tr_local.h"



void R_AddAnimSurfaces( trRefEntity_t *ent ) {
	md4Header_t		*header;
	md4Surface_t	*surface;
	md4LOD_t		*lod;
	shader_t		*shader;
	int				i;

	header = tr.currentModel->md4;
	lod = (md4LOD_t *)( (byte *)header + header->ofsLODs );

	surface = (md4Surface_t *)( (byte *)lod + lod->ofsSurfaces );
	for ( i = 0 ; i < lod->numSurfaces ; i++ ) {
		shader = R_GetShaderByHandle( surface->shaderIndex );
		R_AddDrawSurf( (surfaceType_t*) (void *)surface, shader, 0 /*fogNum*/, qfalse );
		surface = (md4Surface_t *)( (byte *)surface + surface->ofsEnd );
	}
}
