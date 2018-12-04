/*
 * ==================================================================================
 *       Filename:  matrix_multiplication.c
 *    Description:  4*4 matrix 
 * ==================================================================================
 */

#include <xmmintrin.h>
#include "matrix_multiplication.h"



/*
 * out must be 16 byte aliagned,
 */

void MatrixMultiply4x4_SSE(const float A[16], const float B[16], float out[16])
{
    __m128 row1 = _mm_load_ps(&B[0]);
    __m128 row2 = _mm_load_ps(&B[4]);
    __m128 row3 = _mm_load_ps(&B[8]);
    __m128 row4 = _mm_load_ps(&B[12]);
    
    for(int i=0; i<4; i++)
    {
        __m128 brod1 = _mm_set1_ps(A[4*i    ]);
        __m128 brod2 = _mm_set1_ps(A[4*i + 1]);
        __m128 brod3 = _mm_set1_ps(A[4*i + 2]);
        __m128 brod4 = _mm_set1_ps(A[4*i + 3]);
        
        __m128 row = _mm_add_ps(
            _mm_add_ps( _mm_mul_ps(brod1, row1), _mm_mul_ps(brod2, row2) ),
            _mm_add_ps( _mm_mul_ps(brod3, row3), _mm_mul_ps(brod4, row4) )
            );

        _mm_store_ps(&out[4*i], row);
    }
}



//
// NOTE; out = b * a ???
// a, b and c are specified in column-major order
//
void myGlMultMatrix(const float A[16], const float B[16], float out[16])
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

