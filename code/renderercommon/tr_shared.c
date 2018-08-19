/*
===========================================================================
Copyright (C) 2010 James Canete (use.less01@gmail.com)

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
//common function replacements for modular renderer

#include "tr_common.h"

extern refimport_t ri;

/*
const vec4_t	colorBlack	= {0, 0, 0, 1};
const vec4_t	colorRed	= {1, 0, 0, 1};
const vec4_t	colorGreen	= {0, 1, 0, 1};
const vec4_t	colorBlue	= {0, 0, 1, 1};
const vec4_t	colorYellow	= {1, 1, 0, 1};
const vec4_t	colorMagenta= {1, 0, 1, 1};
const vec4_t	colorCyan	= {0, 1, 1, 1};
const vec4_t	colorWhite	= {1, 1, 1, 1};
const vec4_t	colorLtGrey	= {0.75, 0.75, 0.75, 1};
const vec4_t	colorMdGrey	= {0.5, 0.5, 0.5, 1};
const vec4_t	colorDkGrey	= {0.25, 0.25, 0.25, 1};


void AxisClear( vec3_t axis[3] )
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}
*/

union uInt4bytes{
    unsigned int i;
    unsigned char uc[4];
};

char *SkipPath(char *pathname)
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


char *getExtension( const char *name )
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
void PointRotateAroundVector(float* sum, const float* dir, const float* p, const float degrees)
{
    float rad = DEG2RAD( degrees );
    float cos_th = cos( rad );
    float sin_th = sin( rad );
    float d = (1 - cos_th);
    float k[3];

    FastNormalize2f(dir, k);

    d = d * (p[0] * k[0] + p[1] * k[1] + p[2] * k[2]);

	VectorCross(k, p, sum);

    sum[0] *= sin_th;
    sum[1] *= sin_th;
    sum[2] *= sin_th;

    sum[0] += cos_th * p[0]; 
    sum[1] += cos_th * p[1]; 
    sum[2] += cos_th * p[2]; 

    sum[0] += d * k[0];
    sum[1] += d * k[1];
    sum[2] += d * k[2];
}

// note: vector forward are NOT assumed to be nornalized,
// unit: nornalized of forward,
// right: perpendicular of forward 
void MakePerpVectors(const float forward[3], float unit[3], float right[3])
{
	// this rotate and negate guarantees a vector not colinear with the original
    if(forward[0])
    {
        right[0] = 0;
	    right[1] = 1;
	    right[2] = 0;
    }
    else if(forward[1])
    {
        right[0] = 0;
	    right[1] = 0;
	    right[2] = 1;
    }
    else if(forward[2])
    {
        right[0] = 1;
	    right[1] = 0;
	    right[2] = 0;
        return;
    }

    float sqlen = forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2];
    float invLen = 1.0f / sqrtf(sqlen);
 
    unit[0] = forward[0] * invLen;
    unit[1] = forward[1] * invLen;
    unit[2] = forward[2] * invLen;

    
    float d = DotProduct(unit, right);
	right[0] -= d*unit[0];
	right[1] -= d*unit[1];
	right[2] -= d*unit[2];

    // normalize the result
    sqlen = right[0]*right[0] + right[1]*right[1] + right[2]*right[2];
    invLen = 1.0f / sqrtf(sqlen);

    right[0] *= invLen;
    right[1] *= invLen;
    right[2] *= invLen;
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



unsigned ColorBytes4 (float r, float g, float b, float a)
{
	union uInt4bytes cvt;

    cvt.uc[0] = r * 255;
    cvt.uc[1] = g * 255;
    cvt.uc[2] = b * 255;
    cvt.uc[3] = a * 255;

	return cvt.i;
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
