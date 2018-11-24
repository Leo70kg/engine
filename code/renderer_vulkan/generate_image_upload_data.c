#include "tr_local.h"
#include "R_LightScaleTexture.h"
#include "vk_image.h"


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

	int inverseAlpha = 255 - blend[3];
    
	int bR = blend[0] * blend[3];
	int bG = blend[1] * blend[3];
	int bB = blend[2] * blend[3];

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
static void R_MipMap2( unsigned *in, int inWidth, int inHeight )
{
	int	i, j, k;
	
	if ( (inWidth == 1) && (inWidth == 1) )
		return;

	int outWidth = inWidth >> 1;
	int outHeight = inHeight >> 1;
	
    unsigned* temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

	int inWidthMask = inWidth - 1;
	int inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ )
    {
		for ( j = 0 ; j < outWidth ; j++ )
        {
			unsigned char* outpix = (unsigned char *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ )
            {
				int total = 
					1 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((unsigned char *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					1 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((unsigned char *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	memcpy( in, temp, outWidth * outHeight * 4 );
	ri.Hunk_FreeTempMemory( temp );
}





/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap(unsigned char *in, int width, int height)
{

	if ( (width == 1) && (height == 1) )
		return;

    int	i, j;

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

static void ResampleTexture(const unsigned char *pIn, int inwidth, int inheight, unsigned char *pOut, int outwidth, int outheight)
{
	int		i, j;
	unsigned	p1[2048], p2[2048];

	if (outwidth>2048)
		ri.Error(ERR_DROP, "ResampleTexture: max width");
	

	unsigned int fracstep = inwidth*0x10000/outwidth;
	unsigned int frac = fracstep>>2;

	for(i=0; i<outwidth; i++)
    {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ )
    {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0; i<outheight; i++, pOut += outwidth*4)
	{
		unsigned char* inrow = (unsigned char*)pIn + 4*inwidth*(int)((i+0.25)*inheight/outheight);
		unsigned char* inrow2 = (unsigned char*)pIn + 4*inwidth*(int)((i+0.75)*inheight/outheight);
		for (j=0; j<outwidth; j++)
        {
			unsigned char * pix1 = (unsigned char *)inrow + p1[j];
			unsigned char * pix2 = (unsigned char *)inrow + p2[j];

			unsigned char * pix3 = (unsigned char *)inrow2 + p1[j];
			unsigned char * pix4 = (unsigned char *)inrow2 + p2[j];
            
            unsigned char * pCurPix = pOut+j*4;

			pCurPix[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			pCurPix[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			pCurPix[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			pCurPix[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}

    ri.Printf( PRINT_ALL, "ResampleTexture, inwidth: %d, inheight: %d, outwidth: %d, outheight: %d\n",
                inwidth, inheight, outwidth, outheight );

}


void generate_image_upload_data( 
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

	upload_data->buffer = (unsigned char*) ri.Hunk_AllocateTempMemory(2 * 4 * scaled_width * scaled_height);

	unsigned char* resampled_buffer = NULL;
	if ( (scaled_width != width) || (scaled_height != height) )
    {
		resampled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );

		ResampleTexture (data, width, height, resampled_buffer, scaled_width, scaled_height);
		data = resampled_buffer;
		width = scaled_width;
		height = scaled_height;
	}

	//
	// perform optional picmip operation
	//
	if ( picmip ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}

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
	while ( scaled_width > max_texture_size
		|| scaled_height > max_texture_size ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	upload_data->base_level_width = scaled_width;
	upload_data->base_level_height = scaled_height;

	if ( (scaled_width == width) && (scaled_height == height) && !mipmap)
    {
		upload_data->mip_levels = 1;
		upload_data->buffer_size = scaled_width * scaled_height * 4;
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
    unsigned int nBytes = scaled_width * scaled_height * sizeof( unsigned int);
	unsigned char * scaled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( nBytes );
	
    memcpy(scaled_buffer, data, nBytes);
	
    R_LightScaleTexture(scaled_buffer, scaled_width, scaled_height, !mipmap);

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

