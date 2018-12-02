/*
 * =====================================================================================
 *       Filename:  matrix_multiplication.c
 *    Description:  4*4 matrix 
 * =====================================================================================
 */

#include <xmmintrin.h>
#include "matrix_multiplication.h"


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



void MatrixMultiply4x4_SSE(const float A[16], const float B[16], float out[16])
{
    __m128 row1 = _mm_load_ps(&B[0]);
    __m128 row2 = _mm_load_ps(&B[4]);
    __m128 row3 = _mm_load_ps(&B[8]);
    __m128 row4 = _mm_load_ps(&B[12]);
    for(int i=0; i<4; i++)
    {
        __m128 brod1 = _mm_set1_ps(A[4*i + 0]);
        __m128 brod2 = _mm_set1_ps(A[4*i + 1]);
        __m128 brod3 = _mm_set1_ps(A[4*i + 2]);
        __m128 brod4 = _mm_set1_ps(A[4*i + 3]);
        __m128 row = _mm_add_ps(
                    _mm_add_ps(
                        _mm_mul_ps(brod1, row1),
                        _mm_mul_ps(brod2, row2)),
                    _mm_add_ps(
                        _mm_mul_ps(brod3, row3),
                        _mm_mul_ps(brod4, row4)));
        _mm_store_ps(&out[4*i], row);
    }
}


/*
==========================
myGlMultMatrix

==========================

//
// NOTE; out = b * a,
// a, b and c are specified in column-major order
//
void myGlMultMatrix(const float A[16], const float B[16], float out[16] )
{
	int	i, j;

	for ( i = 0 ; i < 4 ; i++ )
    {
		for ( j = 0 ; j < 4 ; j++ )
        {
			out[ i * 4 + j ] =
				  A [ i * 4 + 0 ] * B [ 0 * 4 + j ]
				+ A [ i * 4 + 1 ] * B [ 1 * 4 + j ]
				+ A [ i * 4 + 2 ] * B [ 2 * 4 + j ]
				+ A [ i * 4 + 3 ] * B [ 3 * 4 + j ];
		}
	}
}

*/
