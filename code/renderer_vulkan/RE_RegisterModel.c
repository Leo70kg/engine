#include "tr_local.h"
#include "tr_model.h"
#include "tr_globals.h"



qhandle_t R_RegisterMD4(const char *name, model_t *mod)
{
	ri.Printf(PRINT_WARNING,"Not Impl R_RegisterMD4: name = %s\n", name);
	return 0;
}


qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	
	char* buf;
	int			lod;
	qboolean	loaded = qfalse;
	int			numLoaded;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	char *fext, defex[] = "md3";

	numLoaded = 0;

	strcpy(filename, name);

	fext = strchr(filename, '.');
	if(!fext)
		fext = defex;
	else
	{
		*fext = '\0';
		fext++;
	}

	for (lod = MD3_MAX_LODS - 1 ; lod >= 0 ; lod--)
	{
		if(lod)
			snprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			snprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		ri.R_ReadFile( namebuf, &buf );
		if(!buf)
			continue;
		
#if defined( Q3_BIG_ENDIAN )		
		int ident = LittleLong(*(int *)buf);
#else
		int ident = *(int *)buf;
#endif
		if (ident == MD3_IDENT)
			loaded = R_LoadMD3(mod, lod, buf, name);
		else
			ri.Printf(PRINT_WARNING,"R_RegisterMD3: unknown fileid for %s\n", name);
		
		ri.FS_FreeFile(buf);

		if(loaded)
		{
			mod->numLods++;
			numLoaded++;
		}
		else
			break;
	}

	if(numLoaded)
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for(lod--; lod >= 0; lod--)
		{
			mod->numLods++;
			mod->md3[lod] = mod->md3[lod + 1];
		}

		return mod->index;
	}

#ifdef _DEBUG
	ri.Printf(PRINT_WARNING,"R_RegisterMD3: couldn't load %s\n", name);
#endif

	mod->type = MOD_BAD;
	return 0;
}

/*
====================
R_RegisterMDR
====================
*/
qhandle_t R_RegisterMDR(const char *name, model_t *mod)
{
	
	char* buf;

	qboolean loaded = qfalse;
	int filesize = ri.R_ReadFile(name, &buf);
	if(!buf)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	

#if defined( Q3_BIG_ENDIAN )		
	int ident = LittleLong(*(int *)buf);
#else
	int ident = *(int *)buf;
#endif	

	
	if(ident == MDR_IDENT)
		loaded = R_LoadMDR(mod, buf, filesize, name);

	ri.FS_FreeFile(buf);
	
	if(!loaded)
	{
		ri.Printf(PRINT_WARNING,"R_RegisterMDR: couldn't load mdr file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

/*
====================
R_RegisterIQM
====================
*/
qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	char* buf;
	
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.R_ReadFile(name, &buf);
	if(!buf)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	
	loaded = R_LoadIQM(mod, buf, filesize, name);

	ri.FS_FreeFile (buf);
	
	if(!loaded)
	{
		ri.Printf(PRINT_WARNING,"R_RegisterIQM: couldn't load iqm file %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}


typedef struct
{
	char *ext;
	qhandle_t (*ModelLoader)( const char *, model_t * );
} modelExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple models of different formats available
static const modelExtToLoaderMap_t modelLoaders[ ] =
{
	{ "iqm", R_RegisterIQM },
	{ "mdr", R_RegisterMDR },
	{ "md3", R_RegisterMD3 },
	{ "md4", R_RegisterMD4 }
};

static const int numModelLoaders = ARRAY_LEN(modelLoaders);


/*
====================
Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel( const char *name )
{
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	char		localName[ MAX_QPATH ];
	const char	*ext;


	if ( !name || !name[0] ) {
		ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	//
	// search the currently loaded models
	//
	for ( hModel = 1 ; hModel < tr.numModels; hModel++ ) {
		mod = tr.models[hModel];
		if ( !strcmp( mod->name, name ) ) {
			if( mod->type == MOD_BAD ) {
				return 0;
			}
			return hModel;
		}
	}

	// allocate a new model_t

	if ( ( mod = R_AllocModel() ) == NULL ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );


	// make sure the render thread is stopped
	R_SyncRenderThread();

	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	Q_strncpyz( localName, name, MAX_QPATH );

	ext = getExtension( localName );

	if( *ext )
	{
		int	i;
		// Look for the correct loader and use it
		for( i = 0; i < numModelLoaders; i++ )
		{
			if( !Q_stricmp( ext, modelLoaders[ i ].ext ) )
			{
				// Load
				hModel = modelLoaders[ i ].ModelLoader( localName, mod );
				break;
			}
		}

		// A loader was found
		if( i < numModelLoaders )
		{
			if( !hModel )
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
				return mod->index;
			}
		}
	}

	// Try and find a suitable match using all
	// the model formats supported
    {
        int i;
        for( i = 0; i < numModelLoaders; i++ )
        {
            if (i == orgLoader)
                continue;
            
            char altName[ MAX_QPATH ] = {0};
            snprintf( altName, sizeof (altName), "%s.%s", localName, modelLoaders[ i ].ext );

            // Load
            hModel = modelLoaders[ i ].ModelLoader( altName, mod );

            if( hModel )
            {
                if( orgNameFailed )
                {
                    ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
                            name, altName );
                }

                break;
            }
        }
    }
	return hModel;
}
