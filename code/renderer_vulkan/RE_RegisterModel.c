#include "tr_local.h"
#include "tr_model.h"
#include "tr_globals.h"
#include "../renderercommon/ref_import.h"


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
	R_IssuePendingRenderCommands();

	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	char localName[ MAX_QPATH ] = {0};
	strncpy( localName, name, MAX_QPATH );

	const char* ext = getExtension( localName );

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
		else
		{
			ri.Printf( PRINT_WARNING, "RegisterModel: %s not find \n ", name);
		}

	}
	else
    	ri.Printf( PRINT_WARNING, "RegisterModel: %s without extention. Try and find a suitable match using all the model formats supported\n ", name);
    
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
                    ri.Printf( PRINT_ALL, "WARNING: %s not present, using %s instead\n",
                            name, altName );
                }

                break;
            }
        }
    }
	return hModel;
}
