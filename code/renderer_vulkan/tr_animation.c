/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "tr_globals.h"
#include "tr_cvar.h"

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the 
orientation of the bone in the base frame to the orientation in this
frame.

*/
static float ProjectRadius( float r, vec3_t location )
{

	float c = DotProduct( tr.viewParms.or.axis[0], tr.viewParms.or.origin );
	float dist = DotProduct( tr.viewParms.or.axis[0], location ) - c;

	if ( dist <= 0 )
		return 0;
	vec3_t	p;
	p[0] = 0;
	p[1] = fabs( r );
	p[2] = -dist;

	float projected[4];

/*   
    projected[0] = p[0] * tr.viewParms.projectionMatrix[0] + 
		           p[1] * tr.viewParms.projectionMatrix[4] +
				   p[2] * tr.viewParms.projectionMatrix[8] +
				   tr.viewParms.projectionMatrix[12];

    projected[2] = p[0] * tr.viewParms.projectionMatrix[2] + 
		           p[1] * tr.viewParms.projectionMatrix[6] +
				   p[2] * tr.viewParms.projectionMatrix[10] +
				   tr.viewParms.projectionMatrix[14];
*/

	projected[1] = p[0] * tr.viewParms.projectionMatrix[1] + 
		           p[1] * tr.viewParms.projectionMatrix[5] +
				   p[2] * tr.viewParms.projectionMatrix[9] +
				   tr.viewParms.projectionMatrix[13];


	projected[3] = p[0] * tr.viewParms.projectionMatrix[3] + 
		           p[1] * tr.viewParms.projectionMatrix[7] +
				   p[2] * tr.viewParms.projectionMatrix[11] +
				   tr.viewParms.projectionMatrix[15];


	float pr = projected[1] / projected[3];

	if ( pr > 1.0f )
		pr = 1.0f;

	return pr;
}


static int R_ComputeLOD( trRefEntity_t *ent )
{
	float flod;
	int lod;

	if ( tr.currentModel->numLods < 2 )
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD
	    float radius;

		if(tr.currentModel->type == MOD_MDR)
		{
			mdrHeader_t* mdr = (mdrHeader_t *) tr.currentModel->modelData;
			int frameSize = (size_t) (&((mdrFrame_t *)0)->bones[mdr->numBones]);
			mdrFrame_t* mdrframe = (mdrFrame_t *) ((unsigned char *) mdr + mdr->ofsFrames + frameSize * ent->e.frame);
			
			radius = RadiusFromBounds(mdrframe->bounds[0], mdrframe->bounds[1]);
		}
		else
		{
			md3Frame_t* frame = (md3Frame_t *) (( ( unsigned char * ) tr.currentModel->md3[0] ) + tr.currentModel->md3[0]->ofsFrames);

			frame += ent->e.frame;

			radius = RadiusFromBounds( frame->bounds[0], frame->bounds[1] );
		}

        float projectedRadius = ProjectRadius( radius, ent->e.origin );
		
        if ( projectedRadius != 0 )
		{
			float lodscale = r_lodscale->value;
			if (lodscale > 20)
                lodscale = 20;
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		
        lod = (int)(flod);

		if ( lod < 0 )
		{
			lod = 0;
		}
		else if ( lod >= tr.currentModel->numLods )
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodbias->integer;
	
	if ( lod >= tr.currentModel->numLods )
		lod = tr.currentModel->numLods - 1;
    else if ( lod < 0 )
		lod = 0;

	return lod;
}


static int R_MDRCullModel( mdrHeader_t *header, trRefEntity_t *ent )
{
	vec3_t bounds[2];
	int	i;

	int frameSize = (size_t)( &((mdrFrame_t *)0)->bones[ header->numBones ] );
	
	// compute frame pointers
	mdrFrame_t* newFrame = ( mdrFrame_t * ) ( ( byte * ) header + header->ofsFrames + frameSize * ent->e.frame);
	mdrFrame_t* oldFrame = ( mdrFrame_t * ) ( ( byte * ) header + header->ofsFrames + frameSize * ent->e.oldframe);

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
				// Ummm... yeah yeah I know we don't really have an md3 here.. but we pretend
				// we do. After all, the purpose of mdrs are not that different, are they?
				
				case CULL_OUT:
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;

				case CULL_IN:
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;

				case CULL_CLIP:
					tr.pc.c_sphere_cull_md3_clip++;
					break;
			}
		}
		else
		{
			int sphereCullB;

			int sphereCull = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			if ( newFrame == oldFrame )
			{
				sphereCullB = sphereCull;
			}
			else
			{
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == CULL_OUT )
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				else if ( sphereCull == CULL_IN )
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
	}
	
	// calculate a bounding box in the current coordinate system
	for (i = 0 ; i < 3 ; i++)
    {
		bounds[0][i] = oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
	}

	switch ( R_CullLocalBox( bounds ) )
	{
		case CULL_IN:
			tr.pc.c_box_cull_md3_in++;
			return CULL_IN;
		case CULL_CLIP:
			tr.pc.c_box_cull_md3_clip++;
			return CULL_CLIP;
		case CULL_OUT:
		default:
			tr.pc.c_box_cull_md3_out++;
			return CULL_OUT;
	}
}

static int R_MDRComputeFogNum( mdrHeader_t *header, trRefEntity_t *ent )
{
	vec3_t localOrigin;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}
	
	int frameSize = (size_t)( &((mdrFrame_t *)0)->bones[ header->numBones ] );

	// FIXME: non-normalized axis issues
	mdrFrame_t* mdrFrame = ( mdrFrame_t * ) ( ( byte * ) header + header->ofsFrames + frameSize * ent->e.frame);
	VectorAdd( ent->e.origin, mdrFrame->localOrigin, localOrigin );

    int	i, j;
	for ( i = 1 ; i < tr.world->numfogs ; i++ )
    {
		fog_t* fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ )
        {
			if ( localOrigin[j] - mdrFrame->radius >= fog->bounds[1][j] )
				break;
		
			if ( localOrigin[j] + mdrFrame->radius <= fog->bounds[0][j] )
				break;

		}

		if ( j == 3 )
			return i;
	}

	return 0;
}
// much stuff in there is just copied from R_AddMd3Surfaces in tr_mesh.c

void R_MDRAddAnimSurfaces( trRefEntity_t *ent )
{
	shader_t* shader;
	int	i, j;

	mdrHeader_t* header = (mdrHeader_t *) tr.currentModel->modelData;
	qboolean personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;
	
	if( ent->e.renderfx & RF_WRAP_FRAMES )
	{
		ent->e.frame %= header->numFrames;
		ent->e.oldframe %= header->numFrames;
	}
	
	
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, 
    // so when the surfaces are rendered,
    // they don't need to be range checked again.

	if ((ent->e.frame >= header->numFrames) || (ent->e.frame < 0) ||
		(ent->e.oldframe >= header->numFrames) || (ent->e.oldframe < 0) )
	{
		ri.Printf( PRINT_ALL, "R_MDRAddAnimSurfaces: no such frame %d to %d for '%s'\n", ent->e.oldframe, ent->e.frame, tr.currentModel->name );
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	int cull = R_MDRCullModel(header, ent);
	if ( cull == CULL_OUT ) {
		return;
	}	

	// figure out the current LOD of the model we're rendering, and set the lod pointer respectively.
	int lodnum = R_ComputeLOD(ent);
	// check whether this model has as that many LODs at all. If not, try the closest thing we got.
	if(header->numLODs <= 0)
		return;
	if(header->numLODs <= lodnum)
		lodnum = header->numLODs - 1;

	mdrLOD_t* lod = (mdrLOD_t *)( (unsigned char *)header + header->ofsLODs);
	for(i = 0; i < lodnum; i++)
	{
		lod = (mdrLOD_t *) ((unsigned char *)lod + lod->ofsEnd);
	}
	
	// set up lighting
	if ( !personalModel || r_shadows->integer > 1 )
	{
		R_SetupEntityLighting( &tr.refdef, ent );
	}

	// fogNum?
	int fogNum = R_MDRComputeFogNum( header, ent );

	mdrSurface_t* surface = (mdrSurface_t *)( (unsigned char *)lod + lod->ofsSurfaces );

	for ( i = 0 ; i < lod->numSurfaces ; i++ )
	{
		if(ent->e.customShader)
			shader = R_GetShaderByHandle(ent->e.customShader);
		else if((ent->e.customSkin > 0) && (ent->e.customSkin < tr.numSkins))
		{
			skin_t* skin = tr.skins[ent->e.customSkin];
			shader = tr.defaultShader;
			
			for(j = 0; j < skin->numSurfaces; j++)
			{
				if (0 == strcmp(skin->surfaces[j]->name, surface->name))
				{
					shader = skin->surfaces[j]->shader;
					break;
				}
			}
		}
		else if(surface->shaderIndex > 0)
			shader = R_GetShaderByHandle( surface->shaderIndex );
		else
			shader = tr.defaultShader;

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if ( (personalModel == 0) && (r_shadows->integer == 2) && (fogNum == 0)
			&& !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) )
			&& (shader->sort == SS_OPAQUE) )
		{
			R_AddDrawSurf( (void *)surface, tr.shadowShader, 0, qfalse );
		}

		// projection shadows work fine with personal models
		if ( (r_shadows->integer == 3) && (fogNum == 0)
			&& (ent->e.renderfx & RF_SHADOW_PLANE ) && (shader->sort == SS_OPAQUE) )
		{
			R_AddDrawSurf( (void *)surface, tr.projectionShadowShader, 0, qfalse );
		}

		if (!personalModel)
			R_AddDrawSurf( (void *)surface, shader, fogNum, qfalse );

		surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
	}
}




