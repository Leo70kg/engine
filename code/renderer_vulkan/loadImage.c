/*
 * =====================================================================================
 *
 *       Filename:  loadImage.c
 *
 *    Description:  Loads any of the supported image types into a cannonical 32 bit format.
 *
 *        Version:  1.0
 *        Created:  2018年09月23日 22时13分19秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Sui Jingfeng , jingfengsui@gmail.com
 *   Organization:  CASIA(2014-2017)
 *
 * =====================================================================================
 */

#include "tr_local.h"


#define FILE_HASH_SIZE	1024
static	image_t*		hashTable[FILE_HASH_SIZE];


static int generateHashValue( const char *fname )
{
	int		i = 0;
	int	hash = 0;

	while (fname[i] != '\0') {
		char letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}


typedef struct
{
    char *ext;
    void (*ImageLoader)( const char *, unsigned char **, int *, int * );
} imageExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
const static imageExtToLoaderMap_t imageLoaders[6] =
{
    { "tga",  R_LoadTGA },
    { "jpg",  R_LoadJPG },
    { "jpeg", R_LoadJPG },
    { "png",  R_LoadPNG },
    { "pcx",  R_LoadPCX },
    { "bmp",  R_LoadBMP }
};

const static int numImageLoaders = 6;


void R_LoadImage(const char *name, unsigned char **pic, int *width, int *height )
{

	qboolean orgNameFailed = qfalse;
	int orgLoader = -1;
	int i;
	char localName[ MAX_QPATH ];

	*pic = NULL;
	*width = 0;
	*height = 0;

	Q_strncpyz( localName, name, MAX_QPATH );

	const char *ext = getExtension( localName );


	if( *ext )
	{
		// Look for the correct loader and use it
		for( i = 0; i < numImageLoaders; i++ )
		{
			if( !Q_stricmp( ext, imageLoaders[ i ].ext ) )
			{
				// Load
				imageLoaders[ i ].ImageLoader( localName, pic, width, height );
				break;
			}
		}

		// A loader was found
		if( i < numImageLoaders )
		{
			if( *pic == NULL )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				stripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return;
			}
		}
	}

	// Try and find a suitable match using all
	// the image formats supported
	for( i = 0; i < numImageLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		char *altName = va( "%s.%s", localName, imageLoaders[ i ].ext );

		// Load
		imageLoaders[ i ].ImageLoader( altName, pic, width, height );

		if( *pic )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",name, altName );
			}

			break;
		}
	}
}

/*
		// if we dont get a successful load
	char altname[MAX_QPATH];                              // copy the name
    strcpy( altname, name );                              //
    int len = (int)strlen( altname );                              // 
    altname[len-3] = toupper(altname[len-3]);             // and try upper case extension for unix systems
    altname[len-2] = toupper(altname[len-2]);             //
    altname[len-1] = toupper(altname[len-1]);             //
	ri.Printf( PRINT_ALL, "trying %s...\n", altname );    // 
	R_LoadImage( altname, &pic, &width, &height );        //
*/




image_t* R_FindImageFile(const char *name, qboolean mipmap, 
						qboolean allowPicmip, int glWrapClampMode)
{
	image_t* image;
	int	width, height;
	unsigned char* pic;

	if (!name) {
		return NULL;
	}

	int hash = generateHashValue(name);

	// see if the image is already loaded

	for (image=hashTable[hash]; image; image=image->next)
	{
		if ( !strcmp( name, image->imgName ) )
		{
			// the white image can be used with any set of parms,
			// but other mismatches are errors
			if ( strcmp( name, "*white" ) )
			{
				if ( image->mipmap != mipmap ) {
					ri.Printf( PRINT_ALL, "WARNING: reused image %s with mixed mipmap parm\n", name );
				}
				if ( image->allowPicmip != allowPicmip ) {
					ri.Printf( PRINT_ALL, "WARNING: reused image %s with mixed allowPicmip parm\n", name );
				}
				if ( image->wrapClampMode != glWrapClampMode ) {
					ri.Printf( PRINT_ALL, "WARNING: reused image %s with mixed glWrapClampMode parm\n", name );
				}
			}
			return image;
		}
	}

	//
	// load the pic from disk
    //
    R_LoadImage( name, &pic, &width, &height );
	if (pic == NULL)
	{
        return NULL;
    }

	image = R_CreateImage( ( char * ) name, pic, width, height,
							mipmap, allowPicmip, glWrapClampMode );
	ri.Free( pic );
	return image;
}




//////////////////////////////////////////////////



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
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned *in, int inWidth, int inHeight ) {
	int			i, j, k;
	unsigned char		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (unsigned char *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
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
static void R_MipMap (unsigned char *in, int width, int height)
{
	int		i, j;
	unsigned char	*out;
	int		row;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)in, width, height );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}



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


struct Image_Upload_Data generate_image_upload_data(const unsigned char* data, int width, int height, qboolean mipmap, qboolean picmip)
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

	struct Image_Upload_Data upload_data;
	upload_data.buffer = (unsigned char*) ri.Hunk_AllocateTempMemory(2 * 4 * scaled_width * scaled_height);

	unsigned char* resampled_buffer = NULL;
	if ( scaled_width != width || scaled_height != height ) {
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

	upload_data.base_level_width = scaled_width;
	upload_data.base_level_height = scaled_height;

	if (scaled_width == width && scaled_height == height && !mipmap) {
		upload_data.mip_levels = 1;
		upload_data.buffer_size = scaled_width * scaled_height * 4;
		memcpy(upload_data.buffer, data, upload_data.buffer_size);
		if (resampled_buffer != NULL)
			ri.Hunk_FreeTempMemory(resampled_buffer);
		return upload_data;
	}

	// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
	while (width > scaled_width || height > scaled_height) {
		R_MipMap((unsigned char *)data, width, height);

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1; 
	}

	// At this point width == scaled_width and height == scaled_height.

	unsigned char * scaled_buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );
	memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);
	R_LightScaleTexture(scaled_buffer, scaled_width, scaled_height, (qboolean) !mipmap);

	int miplevel = 0;
	int mip_level_size = scaled_width * scaled_height * 4;

	memcpy(upload_data.buffer, scaled_buffer, mip_level_size);
	upload_data.buffer_size = mip_level_size;
	
	if (mipmap) {
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

			memcpy(&upload_data.buffer[upload_data.buffer_size], scaled_buffer, mip_level_size);
			upload_data.buffer_size += mip_level_size;
		}
	}
	upload_data.mip_levels = miplevel + 1;

	ri.Hunk_FreeTempMemory(scaled_buffer);
	if (resampled_buffer != NULL)
		ri.Hunk_FreeTempMemory(resampled_buffer);

	return upload_data;
}




/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, const byte *pic, int width, int height,
						qboolean mipmap, qboolean allowPicmip, int glWrapClampMode )
{
	if (strlen(name) >= MAX_QPATH ) {
		ri.Error (ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name);
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n");
	}

	// Create image_t object.
	image_t* image = tr.images[tr.numImages] = (image_t*) ri.Hunk_Alloc( sizeof( image_t ), h_low );
    image->index = tr.numImages;
	image->texnum = 1024 + tr.numImages;
	image->mipmap = mipmap;
	image->allowPicmip = allowPicmip;
	strcpy (image->imgName, name);
	image->width = width;
	image->height = height;
	image->wrapClampMode = glWrapClampMode;

	long hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	tr.numImages++;

	// Create corresponding GPU resource.
	qboolean isLightmap = (strncmp(name, "*lightmap", 9) == 0);
	glState.currenttmu = (isLightmap ? 1 : 0);
	
    GL_Bind(image);
	
	struct Image_Upload_Data upload_data = generate_image_upload_data(pic, width, height, 
		mipmap, allowPicmip);


	// VULKAN

	vk_world.images[image->index] = upload_vk_image(&upload_data, glWrapClampMode == GL_REPEAT);


	ri.Hunk_FreeTempMemory(upload_data.buffer);
	return image;
}



void R_InitImages( void )
{
    memset(hashTable, 0, sizeof(hashTable));
    // important

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}


