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



/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, unsigned char* pic, int width, int height,
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

	int hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	tr.numImages++;

	// Create corresponding GPU resource.
	qboolean isLightmap = (strncmp(name, "*lightmap", 9) == 0);
	glState.currenttmu = (isLightmap ? 1 : 0);
	

    if ( glState.currenttextures[glState.currenttmu] != image->texnum )
    {
        glState.currenttextures[glState.currenttmu] = image->texnum;

        image->frameUsed = tr.frameCount;

        // VULKAN
        vk_world.current_descriptor_sets[glState.currenttmu] = 
            vk_world.images[image->index].descriptor_set ;
    }



	struct Image_Upload_Data upload_data;
    memset(&upload_data, 0, sizeof(upload_data));


    generate_image_upload_data(&upload_data, pic, width, height, mipmap, allowPicmip);


	vk_world.images[image->index] = upload_vk_image(&upload_data, glWrapClampMode == GL_REPEAT);

	if (isLightmap) {
		glState.currenttmu = 0;
	}
	ri.Hunk_FreeTempMemory(upload_data.buffer);
	return image;
}



image_t* R_FindImageFile(const char *name, qboolean mipmap, 
						qboolean allowPicmip, int glWrapClampMode)
{

   	image_t* image;
	int	width, height;
	unsigned char* pic;

	if (name == NULL)
    {
        ri.Printf( PRINT_WARNING, "R_FindImageFile: NULL\n");
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
    R_LoadImage2( name, &pic, &width, &height );
	if (pic == NULL)
	{
        return NULL;
    }

	image = R_CreateImage( name, pic, width, height,
							mipmap, allowPicmip, glWrapClampMode );
	ri.Free( pic );
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
