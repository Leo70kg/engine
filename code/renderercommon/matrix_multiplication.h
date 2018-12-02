#ifndef MATRIX_MULTIPLICATION_H_
#define MATRIX_MULTIPLICATION_H_

void MatrixMultiply4x4(const float A[16], const float B[16], float out[16]);
void MatrixMultiply4x4_SSE(const float A[16], const float B[16], float out[16]);
void MatrixMultiply4x4_ASM(const float A[16], const float B[16], float out[16]);
void myGlMultMatrix( const float *a, const float *b, float *out );
#endif
