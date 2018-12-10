#include "tr_local.h"
#include "../renderercommon/ref_import.h"

#include "R_LightScaleTexture.h"
#include "vk_image.h"
#include "tr_cvar.h"


static void imsave(char *fileName, unsigned char* buffer2, unsigned int width, unsigned int height);

static const unsigned char mipBlendColors[16][4] =
{
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};

/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture(unsigned char* data, int pixelCount, const unsigned char blend[4])
{
	int	i;
    int alpha = blend[3];
	int inverseAlpha = 255 - blend[3];
    
	int bR = blend[0] * alpha;
	int bG = blend[1] * alpha;
	int bB = blend[2] * alpha;

	for ( i = 0; i < pixelCount; i++, data+=4 )
    {
		data[0] = ( data[0] * inverseAlpha + bR ) >> 9;
		data[1] = ( data[1] * inverseAlpha + bG ) >> 9;
		data[2] = ( data[2] * inverseAlpha + bB ) >> 9;
	}
}


/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned char * in, unsigned int inWidth, unsigned int inHeight )
{

	int	i, j;

	if ( (inWidth == 1) && (inWidth == 1) )
		return;
	//ri.Printf (PRINT_ALL, "\n---R_MipMap2---\n");
    // Not run time funs, can be used for best view effects

	unsigned int outWidth = inWidth >> 1;
	unsigned int outHeight = inHeight >> 1;
    unsigned int nBytes = outWidth * outHeight * 4;

    unsigned char * temp = ri.Hunk_AllocateTempMemory( nBytes );

	const unsigned int inWidthMask = inWidth - 1;
	const unsigned int inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ )
    {
		for ( j = 0 ; j < outWidth ; j++ )
        {
            unsigned int outIndex = i * outWidth + j;
            unsigned int k;
			for ( k = 0 ; k < 4 ; k++ )
            {
                unsigned int r0 = ((i*2-1) & inHeightMask) * inWidth;
                unsigned int r1 = ((i*2 ) & inHeightMask) * inWidth;
                unsigned int r2 = ((i*2+1) & inHeightMask) * inWidth;
                unsigned int r3 = ((i*2+2) & inHeightMask) * inWidth;

                unsigned int c0 = ((j*2-1) & inWidthMask);
                unsigned int c1 = ((j*2 ) & inWidthMask);
                unsigned int c2 = ((j*2+1) & inWidthMask);
                unsigned int c3 = ((j*2+2) & inWidthMask);


				unsigned int total = 
					1 * in[(r0 + c0) * 4 + k] +
					2 * in[(r0 + c1) * 4 + k] +
					2 * in[(r0 + c2) * 4 + k] +
					1 * in[(r0 + c3) * 4 + k] +

					2 * in[(r1 + c0) * 4 + k] +
					4 * in[(r1 + c1) * 4 + k] +
					4 * in[(r1 + c2) * 4 + k] +
					2 * in[(r1 + c3) * 4 + k] +

					2 * in[(r2 + c0) * 4 + k] +
					4 * in[(r2 + c1) * 4 + k] +
					4 * in[(r2 + c2) * 4 + k] +
					2 * in[(r2 + c3) * 4 + k] +

					1 * in[(r3 + c0) * 4 + k] +
					2 * in[(r3 + c1) * 4 + k] +
					2 * in[(r3 + c2) * 4 + k] +
					1 * in[(r3 + c3) * 4 + k] ;
				
                temp[4*outIndex + k] = (total + 18) / 36;
			}
		}
	}

	memcpy( in, temp, nBytes );
	ri.Hunk_FreeTempMemory( temp );
}





/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap(unsigned char* in, unsigned int width, unsigned int height)
{

	if ( (width == 1) && (height == 1) )
		return;

    unsigned int i;

    const unsigned int row = width * 4;
    unsigned char* out = in;
	width >>= 1;
	height >>= 1;

	if ( (width == 0) || (height == 0) )
    {
		width += height;	// get largest
		for (i=0; i<width; i++, out+=4, in+=8 )
        {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
	}
    else
    {   
        for (i=0; i<height; i++, in+=row)
        {
            unsigned int j;
            for (j=0; j<width; j++, out+=4, in+=8)
            {
                out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
                out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
                out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
                out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
            }
        }
    }
}





/*
================

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function before or after.
================
*/

static void ResampleTexture(const unsigned char *pIn, unsigned int inwidth, unsigned int inheight,
                                    unsigned char *pOut, unsigned int outwidth, unsigned int outheight)
{
	unsigned int i, j;
	unsigned int p1[1024], p2[1024];

	if (outwidth>1024)
		ri.Error(ERR_DROP, "ResampleTexture: max width");
	
    {
        unsigned int fracstep = (inwidth << 16)/outwidth;

        unsigned int frac1 = fracstep>>2;
        unsigned int frac2 = 3*(fracstep>>2);

        for(i=0; i<outwidth; i++)
        {
            p1[i] = 4*(frac1>>16);
            frac1 += fracstep;

            p2[i] = 4*(frac2>>16);
            frac2 += fracstep;
        }
    }

   
	for (i=0; i<outheight; i++)
	{
		const unsigned char* inrow1 = pIn + 4 * inwidth * (unsigned int)((i+0.25)*inheight/outheight);
		const unsigned char* inrow2 = pIn + 4 * inwidth * (unsigned int)((i+0.75)*inheight/outheight);
        //const unsigned char* inrow1 = pIn + inwidth * (unsigned int)((4*i+1)*inheight/outheight);
		//const unsigned char* inrow2 = pIn + inwidth * (unsigned int)((4*i+3)*inheight/outheight);

        /*
        printf("inrow1: %d \t inrow2: %d \n", 
                4 * (unsigned int)((i+0.25)*inheight/outheight),
                4 * (unsigned int)((i+0.75)*inheight/outheight)
                );
        */

        for (j=0; j<outwidth; j++)
        {
			const unsigned char* pix1 = inrow1 + p1[j];
			const unsigned char* pix2 = inrow1 + p2[j];
			const unsigned char* pix3 = inrow2 + p1[j];
			const unsigned char* pix4 = inrow2 + p2[j];
            
            unsigned char* pCurPix = pOut + j*4;

			pCurPix[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			pCurPix[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			pCurPix[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			pCurPix[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}

        pOut += outwidth*4;
	}
}



void generate_image_upload_data(
        const char *name, 
        struct Image_Upload_Data* upload_data, 
        unsigned char* data,
        int width, int height,
        VkBool32 mipmap, VkBool32 picmip)
{

    // ri.Printf (PRINT_ALL, "generate_image_upload_data: %s\n", name);

	//
	// convert to exact power of 2 sizes
	//
	int scaled_width, scaled_height;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;
	
    if ( r_roundImagesDown->integer && scaled_width > width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > height )
		scaled_height >>= 1;


    const unsigned int nBytes = 4 * scaled_width * scaled_height;

	upload_data->buffer = (unsigned char*) ri.Hunk_AllocateTempMemory(2 * nBytes);

	unsigned char* resampled_buffer = NULL;
	if ( (scaled_width != width) || (scaled_height != height) )
    {

		resampled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( nBytes );
        
        ri.Printf( PRINT_ALL, "ResampleTexture, inwidth: %d, inheight: %d, outwidth: %d, outheight: %d\n",
                width, height, scaled_width, scaled_height );
		
        ResampleTexture (data, width, height, resampled_buffer, scaled_width, scaled_height);
    	
        ////
        {
            const char *slash = strrchr(name, '/');

            char tmpName[128] = {0};
            char tmpName2[128] = "resampled_";

            strcpy(tmpName, slash+1);
            strcat(tmpName2, tmpName);

            imsave(tmpName , data, width, height);            
            imsave(tmpName2, resampled_buffer, scaled_width, scaled_height);

            ri.Printf( PRINT_ALL, "tmpName: %s\n", tmpName);
            ri.Printf( PRINT_ALL, "tmpName2: %s\n",  tmpName2);
        }
        ////

        data = resampled_buffer;
		width = scaled_width;
		height = scaled_height;
	}

	//
	// perform optional picmip operation
	/*
	if ( picmip ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}
    */
	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	int max_texture_size = 2048;
	while ( scaled_width > max_texture_size || scaled_height > max_texture_size )
    {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	upload_data->base_level_width = scaled_width;
	upload_data->base_level_height = scaled_height;

	if ( (scaled_width == width) && (scaled_height == height) && !mipmap)
    {
		upload_data->mip_levels = 1;
		upload_data->buffer_size = nBytes;
		memcpy(upload_data->buffer, data, upload_data->buffer_size);
		if (resampled_buffer != NULL)
			ri.Hunk_FreeTempMemory(resampled_buffer);
		
        return;
	}

	// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
	while (width > scaled_width || height > scaled_height)
    {
        if ( r_simpleMipMaps->integer )
        {
            R_MipMap(data, width, height );
        }
        else
        {
            R_MipMap2(data, width, height);
        }

		width >>= 1;
		if (width < 1)
            width = 1;

		height >>= 1;
		if (height < 1)
            height = 1; 
	}


    
	// At this point width == scaled_width and height == scaled_height.
    //unsigned int nBytes = scaled_width * scaled_height * sizeof( unsigned int);
	unsigned char * scaled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( nBytes );
	
    memcpy(scaled_buffer, data, nBytes);
	
    R_LightScaleTexture(scaled_buffer, nBytes, !mipmap);

	int miplevel = 0;

	memcpy(upload_data->buffer, scaled_buffer, nBytes);
	upload_data->buffer_size = nBytes;


	if (mipmap)
    {
		while (scaled_width > 1 || scaled_height > 1)
        {
            if ( r_simpleMipMaps->integer )
            {
                R_MipMap(scaled_buffer, scaled_width, scaled_height);
            }
            else
            {
                R_MipMap2(scaled_buffer, scaled_width, scaled_height);
            }

			scaled_width >>= 1;
			if (scaled_width < 1) scaled_width = 1;

			scaled_height >>= 1;
			if (scaled_height < 1) scaled_height = 1;

			miplevel++;
			unsigned int mip_level_size = scaled_width * scaled_height * 4;

			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( scaled_buffer, scaled_width * scaled_height, mipBlendColors[miplevel] );
			}

			memcpy(upload_data->buffer+upload_data->buffer_size, scaled_buffer, mip_level_size);
			upload_data->buffer_size += mip_level_size;
		}
	}
	upload_data->mip_levels = miplevel + 1;

	ri.Hunk_FreeTempMemory(scaled_buffer);
	
    if (resampled_buffer != NULL)
		ri.Hunk_FreeTempMemory(resampled_buffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////



void fsWriteFile( const char *qpath, const void *buffer, int size )
{
	
    unsigned char* buf = (unsigned char *)buffer;

    FILE * f = fopen( qpath, "wb" );
	if ( !f )
    {
		fprintf(stderr, "Failed to open %s\n", qpath );
		return;
	}

    int remaining = size;
	int tries = 0;
    int block = 0; 
    int written = 0;

    while (remaining)
    {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0)
        {
			if (!tries)
            {
				tries = 1;
			}
            else
            {
				fprintf(stderr, "FS_Write: 0 bytes written\n" );
				return;
			}
		}

		if (written == -1)
        {
			fprintf(stderr, "FS_Write: -1 bytes written\n" );
			return;
		}

		remaining -= written;
		buf += written;
	}

    //FS_Write( buffer, size, f );
    
    fclose(f);
}



static void imsave(char *fileName, unsigned char* buffer2, unsigned int width, unsigned int height)
{

    const unsigned int cnPixels = glConfig.vidWidth * glConfig.vidHeight;

	unsigned char* buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( cnPixels * 3 + 18);

    memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

    unsigned char* buffer_ptr = buffer + 18;
    unsigned char* buffer2_ptr = buffer2;
    
    unsigned int i;
    for (i = 0; i < cnPixels; i++)
    {
        buffer_ptr[0] = buffer2_ptr[0];
        buffer_ptr[1] = buffer2_ptr[1];
        buffer_ptr[2] = buffer2_ptr[2];
        buffer_ptr += 3;
        buffer2_ptr += 4;
    }


	// swap rgb to bgr
	const unsigned int c = 18 + width * height * 3;
	for (i=18; i<c; i+=3)
    {
		unsigned char temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

 	
    fsWriteFile( fileName, buffer, c );

	ri.Hunk_FreeTempMemory( buffer );

    ri.Printf( PRINT_ALL, "imsave: %s\n", fileName );
}

