#ifndef MVP_MARIX_H_
#define MVP_MARIX_H_

void get_modelview_matrix(float mv[16]);
void set_modelview_matrix(float mv[16]);
void reset_modelview_matrix(void);
void get_mvp_transform(float* mvp, int isProj2D);
void R_SetupProjection( float pMatProj[16] );
float ProjectRadius( float r, float location[3], float pMatProj[16] );


#endif
