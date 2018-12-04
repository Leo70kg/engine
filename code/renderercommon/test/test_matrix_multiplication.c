/*
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include<sys/time.h>
#include "../matrix_multiplication.h"

void MatrixMultiply4x4(const float A[16], const float B[16], float out[16]);
void MatrixMultiply4x4_ASM(const float A[16], const float B[16], float out[16]);

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


void MM4x4Print(float M[16])
{
    
    printf("\n");
    int i = 0;
    for(i = 0; i<16; i++)
    {
        if(i%4 == 0)
            printf("\n");

        printf(" %f,\t", M[i]);
    }
    printf("\n");
}



int main(int argc, char *argv[])
{
    struct timeval tv_begin, tv_end;

    int n = 0, netTimeMS = 0;
    
    int runCount = 10000000;

    float A[16] = {
    1.1, 2.2, 3.3, 4.4, 5.5, 6.0, 7.1, 8.3,
    0.1, 0.2, 0.3, 0.4, 2.5, 3.0, 4.1, 5.3,    
    };

    float B[16] = {
    9.1, 3.2, 5.3, 4.3, 5.4, 6.9, 4.1, 1.3,
    0.0, 1.2, 2.3, 7.4, 6.4, 7.0, 6.1, 2.3,    
    };

    float C[16] = {0};
    float out[16] = {0};
    float out2[16] = {0};
    float out3[16] = {0};



    {

        gettimeofday(&tv_begin, NULL);

        for(n = 0; n < runCount; n++ )
        {
            MatrixMultiply4x4(A, B, C);
            A[n%16] = (n%16)/10;
        }
        gettimeofday(&tv_end, NULL);

        netTimeMS = (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec)/1000;

        MM4x4Print(C);
        printf("MatrixMultiply4x4: %d\n", netTimeMS);

    }


    {

        gettimeofday(&tv_begin, NULL);

        for(n = 0; n < runCount; n++ )
        {    
            MatrixMultiply4x4_SSE(A, B, out);
            A[n%16] = (n%16)/10;
        }
        gettimeofday(&tv_end, NULL);

        netTimeMS = (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec)/1000;

        MM4x4Print(out);
        printf("MatrixMultiply4x4_SSE: %d\n", netTimeMS);

    }


    {

        gettimeofday(&tv_begin, NULL);

        for(n = 0; n < runCount; n++ )
        {
            myGlMultMatrix(A, B, out2);
            A[n%16] = (n%16)/10;
        }
        
        gettimeofday(&tv_end, NULL);

        netTimeMS = (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec)/1000;
        MM4x4Print(out2);
        printf("myGlMultMatrix: %d\n", netTimeMS);

    }



    {

        gettimeofday(&tv_begin, NULL);

        for(n = 0; n < runCount; n++ )
        {
            MatrixMultiply4x4_ASM(B, A, out3);
            A[n%16] = (n%16)/10;
        }
        gettimeofday(&tv_end, NULL);

        netTimeMS = (tv_end.tv_sec - tv_begin.tv_sec) * 1000 + (tv_end.tv_usec - tv_begin.tv_usec)/1000;
        MM4x4Print(out3);
        printf("MatrixMultiply4x4_ASM: %d\n", netTimeMS);
    }


    return 1;
}


