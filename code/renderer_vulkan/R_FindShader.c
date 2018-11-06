#include "tr_local.h"


#define MAX_SHADERTEXT_HASH		2048
static char **shaderTextHashTable[MAX_SHADERTEXT_HASH] ={ 0 };

#define FILE_HASH_SIZE		1024
static	shader_t*		hashTable[FILE_HASH_SIZE] = {0};


static char *s_shaderText = NULL;

/*
================
return a hash value for the filename
================
*/
static int generateHashValue( const char *fname, const int size )
{
	int	 i = 0;
	long hash = 0;

	while (fname[i] != '\0')
    {
		char letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		if (letter == PATH_SEP) letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}



void R_ClearShaderHashTable()
{
	memset(hashTable, 0, sizeof(hashTable));
}



/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static char* FindShaderInShaderText( const char *shadername )
{
	int hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

    int i;
	for (i = 0; shaderTextHashTable[hash][i]; i++)
    {
		char * p = shaderTextHashTable[hash][i];
		char* token = R_ParseExt(&p, qtrue);
		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
	}

	char* p = s_shaderText;

	if ( !p ) {
		return NULL;
	}

	// look for label
	while ( 1 )
    {
		char* token = R_ParseExt( &p, qtrue );
		
        if ( token[0] == 0 ) {
			break;
		}

		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
		else {
			// skip the definition, tr_common
            // -> R_SkipBracedSection ?
			SkipBracedSection( &p , 0);
		}
	}

	return NULL;
}



/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as apropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as apropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as apropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage )
{
	char		strippedName[MAX_QPATH] = {0};

	char		*shaderText;
	shader_t	*sh;

	if ( name[0] == 0 )
    {
        ri.Printf( PRINT_WARNING, "R_FindShader: name = NULL\n");
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have lightmaps
	if ( lightmapIndex >= 0 && (lightmapIndex >= tr.numLightmaps) )
    {
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}
    else if ( lightmapIndex < LIGHTMAP_2D )
    {
		// negative lightmap indexes cause stray pointers (think tr.lightmaps[lightmapIndex])
		ri.Printf( PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex  );
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}
    

	stripExtension(name, strippedName, sizeof(strippedName));

    int	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = hashTable[hash]; sh; sh = sh->next)
    {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
		     !Q_stricmp(sh->name, strippedName)) {
			// match found
			return sh;
		}
	}

    R_SetTheShader( strippedName, lightmapIndex );

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText )
    {
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if ( r_printShaders->integer ) {
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}
        
		if ( !ParseShader( &shaderText ) )
        {
			// had errors, so use default shader
            R_SetDefaultShader( );

            ri.Printf( PRINT_WARNING, "ParseShader: %s had errors\n", shaderText );

		}
		sh = FinishShader();
		return sh;
	}

	// if not defined in the in-memory shader descriptions,
	// look for a single supported image file
    char fileName[128] = {0};
    {
        qboolean ptExist = qfalse;
        int i = 0;

        while(name[i] != '\0')
        {
            fileName[i] = name[i];
            if(fileName[i] == '.')
            {
                ptExist = qtrue;
            }
            i++;
        }

        // if path doesn't have an extension, then append
        // the specified one (which should include the .)

        if(ptExist == qtrue)
            fileName[i] = '\0';
        else
        {
            fileName[i++] = '.';
            fileName[i++] = 't';
            fileName[i++] = 'g';
            fileName[i++] = 'a';
            fileName[i] = '\0';
        }
    }

    image_t* image = R_FindImageFile( fileName, mipRawImage, mipRawImage, mipRawImage ? GL_REPEAT : GL_CLAMP );


	//
	// create the default shading commands
	//
    R_CreateDefaultShadingCmds(fileName, image);

    return FinishShader();
}




/* 
====================
This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name )
{

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}


	shader_t* sh = R_FindShader( name, LIGHTMAP_2D, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name )
{

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	shader_t* sh = R_FindShader( name, LIGHTMAP_2D, qfalse );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage)
{

	int hash = generateHashValue(name, FILE_HASH_SIZE);
	
	//
	// see if the shader is already loaded
	//
    shader_t* sh;
	for (sh=hashTable[hash]; sh; sh=sh->next)
    {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
			// index by name
			!Q_stricmp(sh->name, name)) {
			// match found
			return sh->index;
		}
	}


    R_SetTheShader( name, lightmapIndex );


	//
	// create the default shading commands
	//
    R_CreateDefaultShadingCmds(name, image);

	sh = FinishShader();
    return sh->index; 
}


/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
#define	MAX_SHADER_FILES	4096
void ScanAndLoadShaderFiles( void )
{
    ri.Printf( PRINT_ALL, "ScanAndLoadShaderFiles\n" );

	char *buffers[MAX_SHADER_FILES] = {0};
	int numShaderFiles;
	int i;
	char *oldp, *hashMem, *textEnd;
	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;
	char shaderName[128]={0};
	int shaderLine;

	long sum = 0, summand;
	// scan for shader files
	char** shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );

	if ( !shaderFiles || !numShaderFiles )
	{
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
        ri.Printf( PRINT_WARNING, "numShaderFiles > MAX_SHADER_FILES\n" );
	}

	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		char filename[128] = {0};

		snprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		ri.Printf( PRINT_ALL, "...loading '%s'\n", filename );
		summand = ri.R_ReadFile( filename, &buffers[i] );
		
		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );
		
		// Do a simple check on the shader structure in that file to make sure one bad shader file cannot fuck up all other shaders.
		char* p = buffers[i];
		R_BeginParseSession(filename);
		while(1)
		{
			char* token = R_ParseExt(&p, qtrue);
			
            if(0 == *token)
				break;

			Q_strncpyz(shaderName, token, sizeof(shaderName));
			shaderLine = R_GetCurrentParseLine();

			token = R_ParseExt(&p, qtrue);
			if(token[0] != '{' || token[1] != '\0')
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing opening brace",
							filename, shaderName, shaderLine);
				if (token[0])
				{
					ri.Printf(PRINT_WARNING, " (found \"%s\" on line %d)", token, R_GetCurrentParseLine());
				}
				ri.Printf(PRINT_WARNING, ".\n");
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}

			if(!SkipBracedSection(&p, 1))
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing closing brace.\n",
							filename, shaderName, shaderLine);
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}
		}
			
		
		if (buffers[i])
			sum += summand;		
	}

	// build single large buffer
	s_shaderText = ri.Hunk_Alloc( sum + numShaderFiles*2, h_low );
	s_shaderText[ 0 ] = '\0';
	textEnd = s_shaderText;
 
	// free in reverse order, so the temp files are all dumped
	for ( i = numShaderFiles - 1; i >= 0 ; i-- )
	{
		if ( !buffers[i] )
			continue;

		strcat( textEnd, buffers[i] );
		strcat( textEnd, "\n" );
		textEnd += strlen( textEnd );
		ri.FS_FreeFile( buffers[i] );
	}

	R_Compress( s_shaderText );

	// free up memory
	ri.FS_FreeFileList( shaderFiles );

	memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
	size = 0;

	char* p = s_shaderText;
	// look for shader names
	while ( 1 )
    {
		char* token = R_ParseExt( &p, qtrue );
        ri.Printf( PRINT_ALL, "%s\n", token );		


		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection(&p, 0);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = (char*)ri.Hunk_Alloc( size * sizeof(char *), h_low );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++)
    {
		shaderTextHashTable[i] = (char **) hashMem;
		hashMem = ((char *) hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));

	p = s_shaderText;
	// look for shader names
	while ( 1 )
    {
		oldp = p;
		char* token = R_ParseExt( &p, qtrue );

        ri.Printf( PRINT_ALL, "%s\n", token );		


		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;

		SkipBracedSection(&p, 0);
	}

	return;

}


/* 
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/

void R_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset)
{

    shader_t* sh2 = tr.defaultShader;

    //R_FindShaderByName( newShaderName );
    {
        char strippedName2[MAX_QPATH];
	    stripExtension( newShaderName, strippedName2, sizeof(strippedName2) );

	    int hash2 = generateHashValue(strippedName2, FILE_HASH_SIZE);

        // see if the shader is already loaded
        shader_t* pSh = hashTable[hash2];

        while ( pSh )
        {
            // NOTE: if there was no shader or image available with the name strippedName
            // then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
            // have to check all default shaders otherwise for every call to R_FindShader
            // with that same strippedName a new default shader is created.
            if (Q_stricmp(pSh->name, strippedName2) == 0)
            {
                // match found
                sh2 = pSh;
                break;
            }
            pSh=pSh->next;
        }

        if (sh2 == tr.defaultShader)
        {
            qhandle_t h;
            //h = RE_RegisterShaderLightMap(newShaderName, 0);
            pSh = R_FindShader( newShaderName, 0, qtrue );

            if ( pSh->defaultShader )
            {
                h = 0;
            }
            else
            {
                h = pSh->index;
            }

            sh2 = R_GetShaderByHandle(h);

            if( (sh2 == tr.defaultShader) || (sh2 == NULL) )
            {
                ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: shader %s not found\n", newShaderName );
            }
        }
    }
    
    char strippedName[MAX_QPATH];
	stripExtension(shaderName, strippedName, sizeof(strippedName));
	int hash = generateHashValue(strippedName, FILE_HASH_SIZE);
    shader_t* sh = hashTable[hash];
	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	
    while( sh )
    {
		if (Q_stricmp(sh->name, strippedName) == 0)
        {
			if (sh != sh2)
            {
				sh->remappedShader = sh2;
			}
            else
            {
				sh->remappedShader = NULL;
			}
		}
        sh = sh->next;
	}

	if (timeOffset)
    {
		sh2->timeOffset = atof(timeOffset);
	}
}




void R_UpdateShaderHashTable(shader_t* newShader)
{
	int hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;
}
