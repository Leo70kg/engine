#include "tr_local.h"



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
static void R_BlendOverTexture(unsigned char* data, int pixelCount, const unsigned char blend[4] )
{
	int		i;
	int		inverseAlpha;
	int		premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 )
    {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}



void generate_image_upload_data( 
        struct Image_Upload_Data* upload_data, 
        const unsigned char* data,
        int width, int height,
        qboolean mipmap, qboolean picmip)
{
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
	if ( scaled_width != width || scaled_height != height )
    {
		resampled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture ((unsigned*)data, width, height, (unsigned*)resampled_buffer, scaled_width, scaled_height);
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
		R_MipMap((unsigned char *)data, width, height);

		width >>= 1;
		if (width < 1)
            width = 1;

		height >>= 1;
		if (height < 1)
            height = 1; 
	}

	// At this point width == scaled_width and height == scaled_height.

	unsigned char * scaled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );
	memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);
	R_LightScaleTexture(scaled_buffer, scaled_width, scaled_height, !mipmap);

	int miplevel = 0;
	int mip_level_size = scaled_width * scaled_height * 4;

	memcpy(upload_data->buffer, scaled_buffer, mip_level_size);
	upload_data->buffer_size = mip_level_size;
	
	if (mipmap)
    {
		while (scaled_width > 1 || scaled_height > 1) {
			R_MipMap(scaled_buffer, scaled_width, scaled_height);

			scaled_width >>= 1;
			if (scaled_width < 1) scaled_width = 1;

			scaled_height >>= 1;
			if (scaled_height < 1) scaled_height = 1;

			miplevel++;
			mip_level_size = scaled_width * scaled_height * 4;

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

