#include "mvp_matrix.h"
#include "tr_cvar.h"
#include "tr_globals.h"
#include "../renderercommon/matrix_multiplication.h"


static float s_modelview_matrix[16] QALIGN(16);

void set_modelview_matrix(float mv[16])
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


        float projected[4];

        //	projected[0] = p[0] * pMatProj[0] + p[1] * pMatProj[4] + p[2] * pMatProj[8] + pMatProj[12];
        //  projected[1] = p[0] * pMatProj[1] - p[1] * pMatProj[5] + p[2] * pMatProj[9] + pMatProj[13];
        //	projected[2] = p[0] * pMatProj[2] + p[1] * pMatProj[6] + p[2] * pMatProj[10] + pMatProj[14];
        //  projected[3] = p[0] * pMatProj[3] + p[1] * pMatProj[7] + p[2] * pMatProj[11] + pMatProj[15];

        projected[1] = - r * pMatProj[5] - dist * pMatProj[9] + pMatProj[13];
        projected[3] =   r * pMatProj[7] - dist * pMatProj[11] + pMatProj[15];


        pr = projected[1] / projected[3];

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

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL )
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
        int	i;

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
	
    float zNear	= r_znear->value;
	float p10 = -zFar / (zFar - zNear);

    float py = tan(tr.viewParms.fovY * (M_PI / 360.0f));
    float px = tan(tr.viewParms.fovX * (M_PI / 360.0f));


	pMatProj[0] = 1.0f / px;
	pMatProj[4] = 0;
	pMatProj[8] = 0;	// normally 0
	pMatProj[12] = 0;

	pMatProj[1] = 0;
	pMatProj[5] = -1.0f / py;
	pMatProj[9] =  0;
	pMatProj[13] = 0;

	pMatProj[2] = 0;
	pMatProj[6] = 0;
	pMatProj[10] = p10;
	pMatProj[14] = zNear * p10;

	pMatProj[3] = 0;
	pMatProj[7] = 0;
	pMatProj[11] = -1.0f;
	pMatProj[15] = 0;

}


void get_mvp_transform(float* mvp, int isProj2D)
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
