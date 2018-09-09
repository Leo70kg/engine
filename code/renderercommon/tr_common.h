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
#ifndef TR_COMMON_H
#define TR_COMMON_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"


union uInt4bytes{
    unsigned int i;
    unsigned char uc[4];
};

union f32_u {
	float f;
	uint32_t ui;
    unsigned char uc[4];
	struct {
		unsigned int fraction:23;
		unsigned int exponent:8;
		unsigned int sign:1;
	} pack;
};

union f16_u {
	uint16_t ui;
	struct {
		unsigned int fraction:10;
		unsigned int exponent:5;
		unsigned int sign:1;
	} pack;
};


typedef enum
{
	IMGTYPE_COLORALPHA, // for color, lightmap, diffuse, and specular
	IMGTYPE_NORMAL,
	IMGTYPE_NORMALHEIGHT,
	IMGTYPE_DELUXE, // normals are swizzled, deluxe are not
} imgType_t;

typedef enum
{
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CUBEMAP        = 0x0004,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_CLAMPTOEDGE    = 0x0040,
	IMGFLAG_GENNORMALMAP   = 0x0080,
} imgFlags_t;

typedef struct image_s {
	char		imgName[MAX_QPATH];		// game path, including extension
	int			width, height;				// source image
	int			uploadWidth, uploadHeight;	// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	unsigned int texnum;					// gl texture binding

	int			frameUsed;			// for texture usage in frame statistics

	int			internalFormat;
	int			TMU;				// only needed for voodoo2

	imgType_t   type;
	imgFlags_t  flags;

	struct image_s*	next;
} image_t;

// any change in the LIGHTMAP_* defines here MUST be reflected in
// R_FindShader() in tr_bsp.c
#define LIGHTMAP_2D         -4	// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX  -3	// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE -2
#define LIGHTMAP_NONE       -1

// outside of TR since it shouldn't be cleared during ref re-init
extern refimport_t ri;
extern glconfig_t glConfig;	
// These variables should live inside glConfig but can't because of
// compatibility issues to the original ID vms.  If you release a stand-alone
// game and your mod uses tr_types.h from this build you can safely move them
// to the glconfig_t struct.



float R_NoiseGet4f( float x, float y, float z, float t );
void  R_NoiseInit( void );

image_t *R_FindImageFile( const char *name, imgType_t type, imgFlags_t flags );
image_t *R_CreateImage(const char *name, unsigned char* pic, int width, int height, imgType_t type, imgFlags_t flags, int internalFormat);

void R_IssuePendingRenderCommands( void );
qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex );
qhandle_t RE_RegisterShader( const char *name );
qhandle_t RE_RegisterShaderNoMip( const char *name );
qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage);

// font stuff
void R_InitFreeType( void );
void R_DoneFreeType( void );
void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);

/*
=============================================================

IMAGE LOADERS

=============================================================
*/

void R_LoadBMP( const char *name, byte **pic, int *width, int *height );
void R_LoadJPG( const char *name, byte **pic, int *width, int *height );
void R_LoadPCX( const char *name, byte **pic, int *width, int *height );
void R_LoadPNG( const char *name, byte **pic, int *width, int *height );
void R_LoadTGA( const char *name, byte **pic, int *width, int *height );

/*
=============================================================

//common subroutines for render

=============================================================
*/


void PointRotateAroundVector(float* dst, const float* dir, const float* p, const float degrees);
void FastNormalize1f(float v[3]);
char *getExtension( const char *name );
char *SkipPath(char *pathname);
void stripExtension(const char *in, char *out, int destsize);

char* R_ParseExt(char** data_p, qboolean allowLineBreaks);
int R_Compress( char *data_p );
int R_GetCurrentParseLine( void );
void R_BeginParseSession(const char* name);


// note: vector forward are NOT assumed to be nornalized,
// unit: nornalized of forward,
// right: perpendicular of forward 
void MakePerpVectors(const float forward[3], float unit[3], float right[3]);
float MakeTwoPerpVectors(const float forward[3], float right[3], float up[3]);

unsigned int ColorBytes4 (float r, float g, float b, float a);
void MatrixMultiply4x4(const float A[16], const float B[16], float out[16]);
void ClearBounds( vec3_t mins, vec3_t maxs );
qboolean SkipBracedSection (char **program, int depth);

typedef vec_t mat4_t[16];
typedef int ivec2_t[2];
typedef int ivec3_t[3];
typedef int ivec4_t[4];

void Mat4Zero( mat4_t out );
void Mat4Identity( mat4_t out );
void Mat4Copy( const mat4_t in, mat4_t out );
void Mat4Multiply( const mat4_t in1, const mat4_t in2, mat4_t out );
void Mat4Transform( const mat4_t in1, const vec4_t in2, vec4_t out );
qboolean Mat4Compare(const mat4_t a, const mat4_t b);
void Mat4Dump( const mat4_t in );
void Mat4Translation( vec3_t vec, mat4_t out );
void Mat4Ortho( float left, float right, float bottom, float top, float znear, float zfar, mat4_t out );
void Mat4View(vec3_t axes[3], vec3_t origin, mat4_t out);
void Mat4SimpleInverse( const mat4_t in, mat4_t out);

#define VectorCopy2(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1])
#define VectorSet2(v,x,y)       ((v)[0]=(x),(v)[1]=(y));

#define VectorCopy4(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define VectorSet4(v,x,y,z,w)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z),(v)[3]=(w))
#define DotProduct4(a,b)        ((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2] + (a)[3]*(b)[3])
#define VectorScale4(a,b,c)     ((c)[0]=(a)[0]*(b),(c)[1]=(a)[1]*(b),(c)[2]=(a)[2]*(b),(c)[3]=(a)[3]*(b))

#define VectorCopy5(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3],(b)[4]=(a)[4])

#define OffsetByteToFloat(a)    ((float)(a) * 1.0f/127.5f - 1.0f)
#define FloatToOffsetByte(a)    (byte)((a) * 127.5f + 128.0f)
#define ByteToFloat(a)          ((float)(a) * 1.0f/255.0f)
#define FloatToByte(a)          (byte)((a) * 255.0f)

static ID_INLINE int VectorCompare4(const vec4_t v1, const vec4_t v2)
{
	if(v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] || v1[3] != v2[3])
	{
		return 0;
	}
	return 1;
}

static ID_INLINE int VectorCompare5(const vec5_t v1, const vec5_t v2)
{
	if(v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] || v1[3] != v2[3] || v1[4] != v2[4])
	{
		return 0;
	}
	return 1;
}

void VectorLerp( vec3_t a, vec3_t b, float lerp, vec3_t c);


qboolean SpheresIntersect(vec3_t origin1, float radius1, vec3_t origin2, float radius2);
void BoundingSphereOfSpheres(vec3_t origin1, float radius1, vec3_t origin2, float radius2, vec3_t origin3, float *radius3);

#ifndef SGN
#define SGN(x) (((x) >= 0) ? !!(x) : -1)
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(a,b,c) MIN(MAX((a),(b)),(c))
#endif

int NextPowerOfTwo(int in);
unsigned short FloatToHalf(float in);
float HalfToFloat(unsigned short in);


#endif
