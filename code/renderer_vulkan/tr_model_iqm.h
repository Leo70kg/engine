#ifndef TR_MODE_IQM_H_
#define TR_MODE_IQM_H_
#include "../renderercommon/iqm.h"
// inter-quake-model
typedef struct {
	int		num_vertexes;
	int		num_triangles;
	int		num_frames;
	int		num_surfaces;
	int		num_joints;
	int		num_poses;
	struct srfIQModel_s	*surfaces;

	float		*positions;
	float		*texcoords;
	float		*normals;
	float		*tangents;
	byte		*blendIndexes;
	union {
		float	*f;
		byte	*b;
	} blendWeights;
	byte		*colors;
	int		*triangles;

	// depending upon the exporter, blend indices and weights might be int/float
	// as opposed to the recommended byte/byte, for example Noesis exports
	// int/float whereas the official IQM tool exports byte/byte
	byte blendWeightsType; // IQM_UBYTE or IQM_FLOAT

	int		*jointParents;
	float		*jointMats;
	float		*poseMats;
	float		*bounds;
	char		*names;
} iqmData_t;

// inter-quake-model surface
typedef struct srfIQModel_s {
	surfaceType_t	surfaceType;
	char		name[MAX_QPATH];
	shader_t	*shader;
	iqmData_t	*data;
	int		first_vertex, num_vertexes;
	int		first_triangle, num_triangles;
} srfIQModel_t;

qboolean R_LoadIQM (model_t *mod, void *buffer, int filesize, const char *name );
void R_AddIQMSurfaces( trRefEntity_t *ent );
void RB_IQMSurfaceAnim( surfaceType_t *surface );
int R_IQMLerpTag( orientation_t *tag, iqmData_t *data,
                  int startFrame, int endFrame,
                  float frac, const char *tagName );

#endif
