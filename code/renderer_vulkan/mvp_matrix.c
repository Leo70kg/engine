#include "tr_local.h"
#include "mvp_matrix.h"

static float s_modelview_matrix[16] = { 0 };

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
    memset(s_modelview_matrix, 0, 64);
    s_modelview_matrix[3] = s_modelview_matrix[7] = s_modelview_matrix[11] = s_modelview_matrix[15] = 1;
}


void get_mvp_transform(float* mvp)
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

		myGlMultMatrix(s_modelview_matrix, proj, mvp);
	}
}
