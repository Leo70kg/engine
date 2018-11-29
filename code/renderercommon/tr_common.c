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

#include "tr_common.h"


extern refimport_t ri;


char* SkipPath(char *pathname)
{
	char *last = pathname;
    char c;
    do{
        c = *pathname;
    	if (c == '/')
		    last = pathname+1;
        pathname++;
    }while(c);

	return last;
}


void stripExtension(const char *in, char *out, int destsize)
{
	const char *dot = strrchr(in, '.');
    const char *slash = strrchr(in, '/');


	if ((dot != NULL) && ( (slash < dot) || (slash == NULL) ) )
    {
        int len = dot-in+1;
        if(len <= destsize)
            destsize = len;
        else
		    ri.Printf( PRINT_WARNING, "stripExtension: dest size not enough!\n");
    }

    if(in != out)
    	strncpy(out, in, destsize-1);
	
    out[destsize-1] = '\0';
}


char* getExtension( const char *name )
{
	char* dot = strrchr(name, '.');
    char* slash = strrchr(name, '/');

	if ((dot != NULL) && ((slash == NULL) || (slash < dot) ))
		return dot + 1;
	else
		return "";
}



void VectorCross( const vec3_t v1, const vec3_t v2, vec3_t cross )
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}




// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
void FastNormalize1f(float v[3])
{
	// writing it this way allows gcc to recognize that rsqrt can be used
    float invLen = 1.0f / sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

 	v[0] = v[0] * invLen;
	v[1] = v[1] * invLen;
	v[2] = v[2] * invLen;
}

void FastNormalize2f( const float* v, float* out)
{
	// writing it this way allows gcc to recognize that rsqrt can be used
    float invLen = 1.0f / sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

 	out[0] = v[0] * invLen;
	out[1] = v[1] * invLen;
	out[2] = v[2] * invLen;
}

// use Rodrigue's rotation formula
// dir are not assumed to be unit vector
void PointRotateAroundVector(float* res, const float* vec, const float* p, const float degrees)
{
    float rad = DEG2RAD( degrees );
    float cos_th = cos( rad );
    float sin_th = sin( rad );
    float k[3];

	// writing it this way allows gcc to recognize that rsqrt can be used
    float invLen = 1.0f / sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
 	k[0] = vec[0] * invLen;
	k[1] = vec[1] * invLen;
	k[2] = vec[2] * invLen;

    float d = (1 - cos_th) * (p[0] * k[0] + p[1] * k[1] + p[2] * k[2]);

	res[0] = sin_th * (k[1]*p[2] - k[2]*p[1]);
	res[1] = sin_th * (k[2]*p[0] - k[0]*p[2]);
	res[2] = sin_th * (k[0]*p[1] - k[1]*p[0]);

    res[0] += cos_th * p[0] + d * k[0]; 
    res[1] += cos_th * p[1] + d * k[1]; 
    res[2] += cos_th * p[2] + d * k[2]; 
}

// vector k are assumed to be unit
void RotateAroundUnitVector(float* res, const float* k, const float* p, const float degrees)
{
    float rad = DEG2RAD( degrees );
    float cos_th = cos( rad );
    float sin_th = sin( rad );
 
    float d = (1 - cos_th) * (p[0] * k[0] + p[1] * k[1] + p[2] * k[2]);

	res[0] = sin_th * (k[1]*p[2] - k[2]*p[1]);
	res[1] = sin_th * (k[2]*p[0] - k[0]*p[2]);
	res[2] = sin_th * (k[0]*p[1] - k[1]*p[0]);

    res[0] += cos_th * p[0] + d * k[0]; 
    res[1] += cos_th * p[1] + d * k[1]; 
    res[2] += cos_th * p[2] + d * k[2]; 
}


// note: vector forward are NOT assumed to be nornalized,
// unit: nornalized of forward,
// dst: unit vector which perpendicular of forward(src) 
void VectorPerp( const vec3_t src, vec3_t dst )
{
    float unit[3];
    
    float sqlen = src[0]*src[0] + src[1]*src[1] + src[2]*src[2];
    if(0 == sqlen)
    {
        ri.Printf( PRINT_WARNING, "MakePerpVectors: zero vertor input!\n");
        return;
    }

  	dst[1] = -src[0];
	dst[2] = src[1];
	dst[0] = src[2];
	// this rotate and negate try to make a vector not colinear with the original
    // actually can not guarantee, for example
    // forward = (1/sqrt(3), 1/sqrt(3), -1/sqrt(3)),
    // then right = (-1/sqrt(3), -1/sqrt(3), 1/sqrt(3))


    float invLen = 1.0f / sqrtf(sqlen);
    unit[0] = src[0] * invLen;
    unit[1] = src[1] * invLen;
    unit[2] = src[2] * invLen;

    
    float d = DotProduct(unit, dst);
	dst[0] -= d*unit[0];
	dst[1] -= d*unit[1];
	dst[2] -= d*unit[2];

    // normalize the result
    invLen = 1.0f / sqrtf(dst[0]*dst[0] + dst[1]*dst[1] + dst[2]*dst[2]);

    dst[0] *= invLen;
    dst[1] *= invLen;
    dst[2] *= invLen;
}

// Given a normalized forward vector, create two other perpendicular vectors
// note: vector forward are NOT assumed to be nornalized,
// after this funtion is called , forward are nornalized.
// right, up: perpendicular of forward 
float MakeTwoPerpVectors(const float forward[3], float right[3], float up[3])
{

    float sqLen = forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2];
    if(sqLen)
    {
        float nf[3] = {0, 0, 1};
        float invLen = 1.0f / sqrtf(sqLen);
        nf[0] = forward[0] * invLen;
        nf[1] = forward[1] * invLen;
        nf[2] = forward[2] * invLen;

        float adjlen = DotProduct(nf, right);

        // this rotate and negate guarantees a vector
        // not colinear with the original
        right[0] = forward[2] - adjlen * nf[0];
        right[1] = -forward[0] - adjlen * nf[1];
        right[2] = forward[1] - adjlen * nf[2];


        invLen = 1.0f/sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
        right[0] *= invLen;
        right[1] *= invLen;
        right[2] *= invLen;

        // get the up vector with the right hand rules 
        VectorCross(right, nf, up);

        return (sqLen * invLen);
    }
    return 0;
}


/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qboolean PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c)
{
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );

	VectorCross( d2, d1, plane );

	plane[3] = a[0]*plane[0] + a[1]*plane[1] + a[2]*plane[2];

	if((plane[0]*plane[0] + plane[1]*plane[1] + plane[2]*plane[2]) == 0)
    {
		return qfalse;
	}

	return qtrue;
}

void SetPlaneSignbits (cplane_t *out) {
	int	bits = 0;
    int j;

	// for fast box on planeside test
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist[2];
	int		sides, b, i;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	dist[0] = dist[1] = 0;
	if (p->signbits < 8) // >= 8: default case is original code (dist[0]=dist[1]=0)
	{
		for (i=0 ; i<3 ; i++)
		{
			b = (p->signbits >> i) & 1;
			dist[ b] += p->normal[i]*emaxs[i];
			dist[!b] += p->normal[i]*emins[i];
		}
	}

	sides = 0;
	if (dist[0] >= p->dist)
		sides = 1;
	if (dist[1] < p->dist)
		sides |= 2;

	return sides;
}


void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	if ( v[0] < mins[0] ) {
		mins[0] = v[0];
	}
	if ( v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if ( v[1] < mins[1] ) {
		mins[1] = v[1];
	}
	if ( v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if ( v[2] < mins[2] ) {
		mins[2] = v[2];
	}
	if ( v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}


void MatrixMultiply4x4(const float A[16], const float B[16], float out[16])
{
    out[0] = A[0]*B[0] + A[1]*B[4] + A[2]*B[8] + A[3]*B[12];
    out[1] = A[0]*B[1] + A[1]*B[5] + A[2]*B[9] + A[3]*B[13];
    out[2] = A[0]*B[2] + A[1]*B[6] + A[2]*B[10] + A[3]*B[14];
    out[3] = A[0]*B[3] + A[1]*B[7] + A[2]*B[11] + A[3]*B[15];

    out[4] = A[4]*B[0] + A[5]*B[4] + A[6]*B[8] + A[7]*B[12];
    out[5] = A[4]*B[1] + A[5]*B[5] + A[6]*B[9] + A[7]*B[13];
    out[6] = A[4]*B[2] + A[5]*B[6] + A[6]*B[10] + A[7]*B[14];
    out[7] = A[4]*B[3] + A[5]*B[7] + A[6]*B[11] + A[7]*B[15];

    out[8] = A[8]*B[0] + A[9]*B[4] + A[10]*B[8] + A[11]*B[12];
    out[9] = A[8]*B[1] + A[9]*B[5] + A[10]*B[9] + A[11]*B[13];
    out[10] = A[8]*B[2] + A[9]*B[6] + A[10]*B[10] + A[11]*B[14];
    out[11] = A[8]*B[3] + A[9]*B[7] + A[10]*B[11] + A[11]*B[15];

    out[12] = A[12]*B[0] + A[13]*B[4] + A[14]*B[8] + A[15]*B[12];
    out[13] = A[12]*B[1] + A[13]*B[5] + A[14]*B[9] + A[15]*B[13];
    out[14] = A[12]*B[2] + A[13]*B[6] + A[14]*B[10] + A[15]*B[14];
    out[15] = A[12]*B[3] + A[13]*B[7] + A[14]*B[11] + A[15]*B[15];
}


void ClearBounds(vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}




/*
=================
SkipBracedSection

The next token should be an open brace or set depth to 1 if already parsed it.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
qboolean SkipBracedSection(char **program, int depth)
{
	do
    {
		char* token = R_ParseExt(program, qtrue);
		if( token[1] == 0 )
        {
			if( token[0] == '{' )
            {
				depth++;
			}
			else if( token[0] == '}' )
            {
				depth--;
			}
		}
	} while( depth && *program );

	return ( depth == 0 );
}





// tr_extramath.c - extra math needed by the renderer not in qmath.c
// Some matrix helper functions
// FIXME: do these already exist in ioq3 and I don't know about them?

void Mat4Zero( mat4_t out )
{
	out[ 0] = 0.0f; out[ 4] = 0.0f; out[ 8] = 0.0f; out[12] = 0.0f;
	out[ 1] = 0.0f; out[ 5] = 0.0f; out[ 9] = 0.0f; out[13] = 0.0f;
	out[ 2] = 0.0f; out[ 6] = 0.0f; out[10] = 0.0f; out[14] = 0.0f;
	out[ 3] = 0.0f; out[ 7] = 0.0f; out[11] = 0.0f; out[15] = 0.0f;
}

void Mat4Identity( mat4_t out )
{
	out[ 0] = 1.0f; out[ 4] = 0.0f; out[ 8] = 0.0f; out[12] = 0.0f;
	out[ 1] = 0.0f; out[ 5] = 1.0f; out[ 9] = 0.0f; out[13] = 0.0f;
	out[ 2] = 0.0f; out[ 6] = 0.0f; out[10] = 1.0f; out[14] = 0.0f;
	out[ 3] = 0.0f; out[ 7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

void Mat4Copy( const mat4_t in, mat4_t out )
{
	out[ 0] = in[ 0]; out[ 4] = in[ 4]; out[ 8] = in[ 8]; out[12] = in[12]; 
	out[ 1] = in[ 1]; out[ 5] = in[ 5]; out[ 9] = in[ 9]; out[13] = in[13]; 
	out[ 2] = in[ 2]; out[ 6] = in[ 6]; out[10] = in[10]; out[14] = in[14]; 
	out[ 3] = in[ 3]; out[ 7] = in[ 7]; out[11] = in[11]; out[15] = in[15]; 
}

void Mat4Multiply( const mat4_t in1, const mat4_t in2, mat4_t out )
{
	out[ 0] = in1[ 0] * in2[ 0] + in1[ 4] * in2[ 1] + in1[ 8] * in2[ 2] + in1[12] * in2[ 3];
	out[ 1] = in1[ 1] * in2[ 0] + in1[ 5] * in2[ 1] + in1[ 9] * in2[ 2] + in1[13] * in2[ 3];
	out[ 2] = in1[ 2] * in2[ 0] + in1[ 6] * in2[ 1] + in1[10] * in2[ 2] + in1[14] * in2[ 3];
	out[ 3] = in1[ 3] * in2[ 0] + in1[ 7] * in2[ 1] + in1[11] * in2[ 2] + in1[15] * in2[ 3];

	out[ 4] = in1[ 0] * in2[ 4] + in1[ 4] * in2[ 5] + in1[ 8] * in2[ 6] + in1[12] * in2[ 7];
	out[ 5] = in1[ 1] * in2[ 4] + in1[ 5] * in2[ 5] + in1[ 9] * in2[ 6] + in1[13] * in2[ 7];
	out[ 6] = in1[ 2] * in2[ 4] + in1[ 6] * in2[ 5] + in1[10] * in2[ 6] + in1[14] * in2[ 7];
	out[ 7] = in1[ 3] * in2[ 4] + in1[ 7] * in2[ 5] + in1[11] * in2[ 6] + in1[15] * in2[ 7];

	out[ 8] = in1[ 0] * in2[ 8] + in1[ 4] * in2[ 9] + in1[ 8] * in2[10] + in1[12] * in2[11];
	out[ 9] = in1[ 1] * in2[ 8] + in1[ 5] * in2[ 9] + in1[ 9] * in2[10] + in1[13] * in2[11];
	out[10] = in1[ 2] * in2[ 8] + in1[ 6] * in2[ 9] + in1[10] * in2[10] + in1[14] * in2[11];
	out[11] = in1[ 3] * in2[ 8] + in1[ 7] * in2[ 9] + in1[11] * in2[10] + in1[15] * in2[11];

	out[12] = in1[ 0] * in2[12] + in1[ 4] * in2[13] + in1[ 8] * in2[14] + in1[12] * in2[15];
	out[13] = in1[ 1] * in2[12] + in1[ 5] * in2[13] + in1[ 9] * in2[14] + in1[13] * in2[15];
	out[14] = in1[ 2] * in2[12] + in1[ 6] * in2[13] + in1[10] * in2[14] + in1[14] * in2[15];
	out[15] = in1[ 3] * in2[12] + in1[ 7] * in2[13] + in1[11] * in2[14] + in1[15] * in2[15];
}

void Mat4Transform( const mat4_t in1, const vec4_t in2, vec4_t out )
{
	out[ 0] = in1[ 0] * in2[ 0] + in1[ 4] * in2[ 1] + in1[ 8] * in2[ 2] + in1[12] * in2[ 3];
	out[ 1] = in1[ 1] * in2[ 0] + in1[ 5] * in2[ 1] + in1[ 9] * in2[ 2] + in1[13] * in2[ 3];
	out[ 2] = in1[ 2] * in2[ 0] + in1[ 6] * in2[ 1] + in1[10] * in2[ 2] + in1[14] * in2[ 3];
	out[ 3] = in1[ 3] * in2[ 0] + in1[ 7] * in2[ 1] + in1[11] * in2[ 2] + in1[15] * in2[ 3];
}

qboolean Mat4Compare( const mat4_t a, const mat4_t b )
{
	return !(a[ 0] != b[ 0] || a[ 4] != b[ 4] || a[ 8] != b[ 8] || a[12] != b[12] ||
             a[ 1] != b[ 1] || a[ 5] != b[ 5] || a[ 9] != b[ 9] || a[13] != b[13] ||
		     a[ 2] != b[ 2] || a[ 6] != b[ 6] || a[10] != b[10] || a[14] != b[14] ||
		     a[ 3] != b[ 3] || a[ 7] != b[ 7] || a[11] != b[11] || a[15] != b[15]);
}

void Mat4Dump( const mat4_t in )
{
	ri.Printf(PRINT_ALL, "%3.5f %3.5f %3.5f %3.5f\n", in[ 0], in[ 4], in[ 8], in[12]);
	ri.Printf(PRINT_ALL, "%3.5f %3.5f %3.5f %3.5f\n", in[ 1], in[ 5], in[ 9], in[13]);
	ri.Printf(PRINT_ALL, "%3.5f %3.5f %3.5f %3.5f\n", in[ 2], in[ 6], in[10], in[14]);
	ri.Printf(PRINT_ALL, "%3.5f %3.5f %3.5f %3.5f\n", in[ 3], in[ 7], in[11], in[15]);
}

void Mat4Translation( vec3_t vec, mat4_t out )
{
	out[ 0] = 1.0f; out[ 4] = 0.0f; out[ 8] = 0.0f; out[12] = vec[0];
	out[ 1] = 0.0f; out[ 5] = 1.0f; out[ 9] = 0.0f; out[13] = vec[1];
	out[ 2] = 0.0f; out[ 6] = 0.0f; out[10] = 1.0f; out[14] = vec[2];
	out[ 3] = 0.0f; out[ 7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

void Mat4Ortho( float left, float right, float bottom, float top, float znear, float zfar, mat4_t out )
{
	out[ 0] = 2.0f / (right - left); out[ 4] = 0.0f;                  out[ 8] = 0.0f;                  out[12] = -(right + left) / (right - left);
	out[ 1] = 0.0f;                  out[ 5] = 2.0f / (top - bottom); out[ 9] = 0.0f;                  out[13] = -(top + bottom) / (top - bottom);
	out[ 2] = 0.0f;                  out[ 6] = 0.0f;                  out[10] = 2.0f / (zfar - znear); out[14] = -(zfar + znear) / (zfar - znear);
	out[ 3] = 0.0f;                  out[ 7] = 0.0f;                  out[11] = 0.0f;                  out[15] = 1.0f;
}

void Mat4View(vec3_t axes[3], vec3_t origin, mat4_t out)
{
	out[0]  = axes[0][0];
	out[1]  = axes[1][0];
	out[2]  = axes[2][0];
	out[3]  = 0;

	out[4]  = axes[0][1];
	out[5]  = axes[1][1];
	out[6]  = axes[2][1];
	out[7]  = 0;

	out[8]  = axes[0][2];
	out[9]  = axes[1][2];
	out[10] = axes[2][2];
	out[11] = 0;

	out[12] = -DotProduct(origin, axes[0]);
	out[13] = -DotProduct(origin, axes[1]);
	out[14] = -DotProduct(origin, axes[2]);
	out[15] = 1;
}

void Mat4SimpleInverse( const mat4_t in, mat4_t out)
{
	vec3_t v;
	float invSqrLen;
 
	VectorCopy(in + 0, v);
	invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	out[ 0] = v[0]; out[ 4] = v[1]; out[ 8] = v[2]; out[12] = -DotProduct(v, &in[12]);

	VectorCopy(in + 4, v);
	invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	out[ 1] = v[0]; out[ 5] = v[1]; out[ 9] = v[2]; out[13] = -DotProduct(v, &in[12]);

	VectorCopy(in + 8, v);
	invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	out[ 2] = v[0]; out[ 6] = v[1]; out[10] = v[2]; out[14] = -DotProduct(v, &in[12]);

	out[ 3] = 0.0f; out[ 7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

void VectorLerp( vec3_t a, vec3_t b, float lerp, vec3_t c)
{
	c[0] = a[0] * (1.0f - lerp) + b[0] * lerp;
	c[1] = a[1] * (1.0f - lerp) + b[1] * lerp;
	c[2] = a[2] * (1.0f - lerp) + b[2] * lerp;
}

qboolean SpheresIntersect(vec3_t origin1, float radius1, vec3_t origin2, float radius2)
{
	float radiusSum = radius1 + radius2;
	vec3_t diff;
	
	VectorSubtract(origin1, origin2, diff);

	if (DotProduct(diff, diff) <= radiusSum * radiusSum)
	{
		return qtrue;
	}

	return qfalse;
}

void BoundingSphereOfSpheres(vec3_t origin1, float radius1, vec3_t origin2, float radius2, vec3_t origin3, float *radius3)
{
	vec3_t diff;

	VectorScale(origin1, 0.5f, origin3);
	VectorMA(origin3, 0.5f, origin2, origin3);

	VectorSubtract(origin1, origin2, diff);
	*radius3 = VectorLen(diff) * 0.5f + MAX(radius1, radius2);
}

int NextPowerOfTwo(int in)
{
	int out;

	for (out = 1; out < in; out <<= 1)
		;

	return out;
}


uint16_t FloatToHalf(float in)
{
	union f32_u f32;
	union f16_u f16;

	f32.f = in;

	f16.pack.exponent = CLAMP((int)(f32.pack.exponent) - 112, 0, 31);
	f16.pack.fraction = f32.pack.fraction >> 13;
	f16.pack.sign     = f32.pack.sign;

	return f16.ui;
}


float HalfToFloat(uint16_t in)
{
	union f32_u f32;
	union f16_u f16;

	f16.ui = in;

	f32.pack.exponent = (int)(f16.pack.exponent) + 112;
	f32.pack.fraction = f16.pack.fraction << 13;
	f32.pack.sign = f16.pack.sign;

	return f32.f;
}



//
// tr_subs.c - common function replacements for modular renderer
// 
#ifdef USE_RENDERER_DLOPEN

void QDECL Com_Printf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_Error( int level, const char *error, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	ri.Error(level, "%s", text);
}

#endif








/*
============================================================================

                         PARSING

split those parsing functions from q_shared.c
I want the render part standalone, dont fuck up with game part.

============================================================================
*/


static char	r_parsename[512] = {0};
static int	r_lines = 0;
static int	r_tokenline = 0;

void R_BeginParseSession(const char* name)
{
	r_lines = 1;
	r_tokenline = 0;
	snprintf(r_parsename, sizeof(r_parsename), "%s", name);
}

int R_GetCurrentParseLine( void )
{
	if ( r_tokenline )
	{
		return r_tokenline;
	}

	return r_lines;
}



int R_Compress( char *data_p )
{
	qboolean newline = qfalse;
    qboolean whitespace = qfalse;

	char* in = data_p;
    char* out = data_p;

	if (in)
    {
        int c;
		while ((c = *in) != 0)
        {
			// skip double slash comments
			if ( c == '/' && in[1] == '/' )
            {
				while (*in && *in != '\n') {
					in++;
				}
			// skip /* */ comments
			}
            else if ( c == '/' && in[1] == '*' ) {
				while ( *in && ( *in != '*' || in[1] != '/' ) ) 
					in++;
				if ( *in ) 
					in += 2;
				// record when we hit a newline
			}
            else if ( c == '\n' || c == '\r' ) {
				newline = qtrue;
				in++;
				// record when we hit whitespace
			}
            else if ( (c == ' ') || (c == '\t') )
            {
				whitespace = qtrue;
				in++;
				// an actual token
			}
            else
            {
				// if we have a pending newline, emit it (and it counts as whitespace)
				if (newline)
                {
					*out++ = '\n';
					newline = qfalse;
					whitespace = qfalse;
				}
                if (whitespace)
                {
					*out++ = ' ';
					whitespace = qfalse;
				}

				// copy quoted strings unmolested
				if (c == '"') {
					*out++ = c;
					in++;
					while (1) {
						c = *in;
						if (c && c != '"') {
							*out++ = c;
							in++;
						} else {
							break;
						}
					}
					if (c == '"') {
						*out++ = c;
						in++;
					}
				} else {
					*out = c;
					out++;
					in++;
				}
			}
		}

		*out = 0;
	}
	return out - data_p;
}


/*
==============
Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is a newline.
==============
*/
char* R_ParseExt(char** data_p, qboolean allowLineBreaks)
{

    unsigned int len = 0;
	char *data = *data_p;

    unsigned char c;
    static char r_token[512] = {0}; 
    r_token[0] = 0;
	r_tokenline = 0;

	// make sure incoming data is valid
	if( !data )
	{
		*data_p = NULL;
		return r_token;
	}


	while( 1 )
	{
		// skip whitespace
		//data = SkipWhitespace( data, &hasNewLines );

	    while( (c = *data) <= ' ')
        {
		    if( c == '\n' )
            {
			    r_lines++;
		        if( allowLineBreaks == qfalse )
		        {
			        *data_p = data;
			        return r_token;
		        }
		    }
            else if( c == 0 )
            {
			    *data_p = NULL;
			    return r_token;
		    }

		    data++;
	    }

		// skip double slash comments
		if(data[0] == '/')
        {    
            if(data[1] == '/')
		    {
			    data += 2;
			    while (*data && (*data != '\n'))
                {
				    data++;
			    }
		    }
		    else if( data[1] == '*' ) 
		    {   // skip /* */ comments
			    data += 2;
                // Assuming /* and */ occurs in pairs.
			    while( (data[0] != '*') || (data[1] != '/') ) 
			    {
				    if ( data[0] == '\n' )
				    {
					    r_lines++;
				    }
				    data++;
			    }
				data += 2;
		    }
		    else
                break;
        }
        else
            break;
	}

	// token starts on this line
	r_tokenline = r_lines;

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				r_token[len] = 0;
				*data_p = data;
				return r_token;
			}
            else if ( c == '\n' )
			{
				r_lines++;
			}

			if (len < MAX_TOKEN_CHARS - 1)
			{
				r_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			r_token[len++] = c;
		}

		c = *(++data);
	} while(c > ' ');

	r_token[len] = 0;

	*data_p = data;
	return r_token;
}
