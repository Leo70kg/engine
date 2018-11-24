/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "sdl_icon.h"
#include "qvk.h"
#include "VKimpl.h"
#include "tr_local.h"
#include "tr_displayResolution.h"
#include "Vk_Instance.h"

#ifdef _WIN32
	#include "../SDL2/include/SDL.h"
    #include "../SDL2/include/SDL_vulkan.h"
#else
	#include <SDL2/SDL.h>
    #include <SDL2/SDL_syswm.h>
    #include <SDL2/SDL_vulkan.h>
#endif


static SDL_Window *SDL_window = NULL;

static cvar_t* r_allowResize; // make window resizable


// display refresh rate
static cvar_t* r_displayRefresh;

// not used cvar, keep it for backward compatibility
static cvar_t *r_sdlDriver;
static cvar_t* r_displayIndex;

cvar_t	*r_fullscreen;


static void VKimp_DetectAvailableModes(void)
{
	int i, j;
	char buf[ MAX_STRING_CHARS ] = { 0 };

	SDL_DisplayMode windowMode;
    
	// If a window exists, note its display index
	if( SDL_window != NULL )
	{
		r_displayIndex->integer = SDL_GetWindowDisplayIndex( SDL_window );
		if( r_displayIndex->integer < 0 )
		{
			ri.Printf(PRINT_ALL, "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
            return;
		}
	}

	int numSDLModes = SDL_GetNumDisplayModes( r_displayIndex->integer );

	if( SDL_GetWindowDisplayMode( SDL_window, &windowMode ) < 0 || numSDLModes <= 0 )
	{
		ri.Printf(PRINT_ALL, "Couldn't get window display mode, no resolutions detected: %s\n", SDL_GetError() );
		return;
	}

	int numModes = 0;
	SDL_Rect* modes = SDL_calloc(numSDLModes, sizeof( SDL_Rect ));
	if ( !modes )
	{
        ////////////////////////////////////
		ri.Error(ERR_FATAL, "Out of memory" );
        ////////////////////////////////////
	}

	for( i = 0; i < numSDLModes; i++ )
	{
		SDL_DisplayMode mode;

		if( SDL_GetDisplayMode( r_displayIndex->integer, i, &mode ) < 0 )
			continue;

		if( !mode.w || !mode.h )
		{
			ri.Printf(PRINT_ALL,  "Display supports any resolution\n" );
			SDL_free( modes );
			return;
		}

		if( windowMode.format != mode.format )
			continue;

		// SDL can give the same resolution with different refresh rates.
		// Only list resolution once.
		for( j = 0; j < numModes; j++ )
		{
			if( (mode.w == modes[ j ].w) && (mode.h == modes[ j ].h) )
				break;
		}

		if( j != numModes )
			continue;

		modes[ numModes ].w = mode.w;
		modes[ numModes ].h = mode.h;
		numModes++;
	}

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ].w, modes[ i ].h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			ri.Printf(PRINT_ALL,  "Skipping mode %ux%u, buffer too small\n", modes[ i ].w, modes[ i ].h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		ri.Printf(PRINT_ALL, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
	SDL_free( modes );
}


static int VKimp_SetMode(int mode, qboolean fullscreen)
{
	SDL_DisplayMode desktopMode;

	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN;

	if ( r_allowResize->integer )
		flags |= SDL_WINDOW_RESIZABLE;

	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	ri.Printf(PRINT_ALL,  "...VKimp_SetMode()...\n");


#ifdef USE_ICON
SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
			0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
			);
#endif


    SDL_GetNumVideoDisplays();

	int display_mode_count = SDL_GetNumDisplayModes(r_displayIndex->integer);
	if (display_mode_count < 1)
	{
		ri.Printf(PRINT_ALL, "SDL_GetNumDisplayModes failed: %s", SDL_GetError());
	}


    int tmp = SDL_GetDesktopDisplayMode(r_displayIndex->integer, &desktopMode);
	if( (tmp == 0) && (desktopMode.h > 0) )
    {
    	Uint32 f = desktopMode.format;
        ri.Printf(PRINT_ALL, "bpp %i\t%s\t%i x %i, refresh_rate: %dHz\n", SDL_BITSPERPIXEL(f), SDL_GetPixelFormatName(f), desktopMode.w, desktopMode.h, desktopMode.refresh_rate);
    }
    else if (SDL_GetDisplayMode(r_displayIndex->integer, 0, &desktopMode) != 0)
	{
    	//mode = 0: use the first display mode SDL return;
        ri.Printf(PRINT_ALL,"SDL_GetDisplayMode failed: %s\n", SDL_GetError());
	}


	glConfig.refresh_rate = desktopMode.refresh_rate;


	if (mode == -2)
	{
        // use desktop video resolution
        glConfig.vidWidth = desktopMode.w;
        glConfig.vidHeight = desktopMode.h;
        glConfig.windowAspect = (float)desktopMode.w / (float)desktopMode.h;
        glConfig.refresh_rate = desktopMode.refresh_rate;
    }
	else
	{
        R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, 
                        &glConfig.windowAspect, mode );
    }
    
    ri.Printf(PRINT_ALL,"Display mode: %d\n", mode);


	// Center window
	if(!fullscreen)
	{
		x = ( desktopMode.w / 2 ) - ( glConfig.vidWidth / 2 );
		y = ( desktopMode.h / 2 ) - ( glConfig.vidHeight / 2 );
	}


	if( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		ri.Printf(PRINT_ALL,  "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if( fullscreen )
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		flags |= SDL_WINDOW_BORDERLESS;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		glConfig.isFullscreen = qfalse;
	}


	SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE, x, y,
						glConfig.vidWidth, glConfig.vidHeight, flags );
	if( SDL_window == NULL )
		ri.Error(ERR_FATAL,"SDL_CreateWindow failed: %s\n", SDL_GetError( ) );
    else{
        ri.Printf(PRINT_ALL, "SDL_CreateWindow successed.\n");
    }
#ifdef USE_ICON
	SDL_SetWindowIcon( SDL_window, icon );
#endif



#ifdef USE_ICON
    SDL_FreeSurface( icon );
#endif

	if( SDL_window )
    {
        VKimp_DetectAvailableModes();
        return 0;
    }
    else
	{
		ri.Printf(PRINT_ALL, "Couldn't get a visual\n" );
	}

    return -1;
}



void checkInstanceExt(void)
{
	// check extensions availability
	unsigned int nInsExt = 0;
    
	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &nInsExt, NULL) );

    if (nInsExt > 0)
    {
		ri.Printf(PRINT_ALL, "total %d instance extensions.\n", nInsExt);

        VkExtensionProperties *pInsExt = (VkExtensionProperties *)
            malloc(sizeof(VkExtensionProperties) * nInsExt);

        VK_CHECK(qvkEnumerateInstanceExtensionProperties( NULL, &nInsExt, pInsExt));
            
        unsigned int i = 0;
        for (i = 0; i < nInsExt; i++)
        {
            //ri.Printf(PRINT_ALL, "%s\n", pInsExt[i].extensionName );
            
            strcat(glConfig.extensions_string, pInsExt[i].extensionName);
            strcat(glConfig.extensions_string, "\n");
        }
            
        free(pInsExt);
    }

}




void VKimp_CreateInstance(void)
{
    ri.Printf(PRINT_ALL, "...VKimp_CreateInstance...\n");
	
    VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "OpenArena";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "OpenArena";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

	VkInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

    ////////////////

	// check extensions availability
	unsigned int instance_extension_count = 0;
    const char** instance_extensions = NULL;

    if(!SDL_Vulkan_GetInstanceExtensions(SDL_window, &instance_extension_count, NULL))
	    ri.Error(ERR_FATAL, "Vulkan: SDL_Vulkan_GetInstanceExtensions\n");

    if (instance_extension_count > 0)
    {
        instance_extensions = malloc(
                sizeof(const char *) * (instance_extension_count+1) );
        SDL_Vulkan_GetInstanceExtensions(SDL_window, &instance_extension_count, instance_extensions);
        

        instance_extensions[instance_extension_count] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    }

    instanceCreateInfo.enabledExtensionCount = instance_extension_count;
	instanceCreateInfo.ppEnabledExtensionNames = instance_extensions;


#ifndef NDEBUG
	const char* const validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	ri.Printf(PRINT_ALL, "Using VK_LAYER_LUNARG_standard_validation\n");
    instanceCreateInfo.enabledExtensionCount = instance_extension_count+1;

	instanceCreateInfo.enabledLayerCount = 1;
	instanceCreateInfo.ppEnabledLayerNames = &validation_layer_name;
#endif


    checkInstanceExt();


    VkResult e = qvkCreateInstance(&instanceCreateInfo, NULL, &vk.instance);
    if(!e)
    {
        ri.Printf(PRINT_ALL, "---Vulkan create instance success---\n\n");
    }
    else if (e == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		// The requested version of Vulkan is not supported by the driver 
		// or is otherwise incompatible for implementation-specific reasons.
        ri.Error(ERR_FATAL, 
            "The requested version of Vulkan is not supported by the driver.\n" );
    }
    else if (e == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        ri.Error(ERR_FATAL, "Cannot find a specified extension library.\n");
    }
    else 
    {
        ri.Error(ERR_FATAL, "%d, returned by qvkCreateInstance.\n", e);
    }

    free(instance_extensions);
}



/*
 * This routine is responsible for initializing the OS specific portions of Vulkan
 */
void VKimp_Init()
{
	ri.Printf(PRINT_ALL, "...Initializing Vulkan subsystem...\n");

	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );

	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "", CVAR_ROM );

	r_displayIndex = ri.Cvar_Get( "r_displayIndex", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_displayRefresh = ri.Cvar_Get( "r_displayRefresh", "60", CVAR_LATCH );
    r_fullscreen = ri.Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );


	ri.Cvar_CheckRange( r_displayRefresh, 0, 200, qtrue );

	if(ri.Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		ri.Cvar_Set( "r_fullscreen", "0" );
		ri.Cvar_Set( "com_abnormalExit", "0" );
	}

  
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			ri.Printf(PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
		}
        else
        {
    		ri.Printf(PRINT_ALL, " SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver( ));
        }
    }

	if( 0 == VKimp_SetMode(r_mode->integer, r_fullscreen->integer) )
	{
        goto success;
	}
    else
    {
        ri.Printf(PRINT_ALL, "Setting r_mode=%d, r_fullscreen=%d failed, falling back on r_mode=%d\n",
                r_mode->integer, r_fullscreen->integer, 3 );

        if( 0 == VKimp_SetMode(3, qfalse) )
        {
            goto success;
        }
        else
        {
            ri.Error(ERR_FATAL, "VKimp_Init() - could not load Vulkan subsystem" );
        }
    }


success:

	SDL_Vulkan_LoadLibrary(NULL);    
    // Create the window 

    qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
    if( qvkGetInstanceProcAddr == NULL)
    {
        ri.Error(ERR_FATAL, "Failed to find entrypoint vkGetInstanceProcAddr\n"); 
    }


    // These values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

    // Only using SDL_SetWindowBrightness to determine if hardware gamma is supported
    glConfig.deviceSupportsGamma = qtrue;

    glConfig.textureEnvAddAvailable = qtrue;

    glConfig.textureCompression = TC_NONE;

	// init command buffers and SMP
	glConfig.smpActive = qfalse;
	glConfig.stereoDisabled = qfalse;



	ri.Printf(PRINT_ALL,  "MODE: %s, %d x %d, refresh rate: %dhz\n",
        ((r_fullscreen->integer == 1) ? "fullscreen" : "windowed"), 
        glConfig.vidWidth, glConfig.vidHeight, glConfig.refresh_rate);

    
	// This depends on SDL_INIT_VIDEO, hence having it here
	ri.IN_Init(SDL_window, 0);
}


void VKimp_Shutdown( void )
{
    ri.Printf(PRINT_ALL, "VKimp_Shutdown()\n");

    //Sys_UnloadLibrary(vk_library_handle);
	
    memset(&glConfig, 0, sizeof(glConfig));

    SDL_DestroyWindow( SDL_window );
    SDL_window = NULL;

	ri.IN_Shutdown();
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}


void VKimp_CreateSurface(void)
{
    ri.Printf(PRINT_ALL, "...CreateSurface...\n");

    if(!SDL_Vulkan_CreateSurface(SDL_window, vk.instance, &vk.surface))
    {
        vk.surface = VK_NULL_HANDLE;
        ri.Error(ERR_FATAL, "SDL_Vulkan_CreateSurface(): %s\n", SDL_GetError());
    }
}

void VKimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	Uint16 table[3][256];
	int i, j;

	for (i = 0; i < 256; i++)
	{
		table[0][i] = ( red[i] << 8 ) | red[i];
		table[1][i] = ( green[i] << 8 ) | green[i];
		table[2][i] = ( blue[i]  << 8 ) | blue[i];
	}


	// enforce constantly increasing
	for (j = 0; j < 3; j++)
	{
		for (i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i-1])
				table[j][i] = table[j][i-1];
		}
	}

	if (SDL_SetWindowGammaRamp(SDL_window, table[0], table[1], table[2]) < 0)
	{
		ri.Printf(PRINT_ALL, "SDL_SetWindowGammaRamp() failed: %s\n", SDL_GetError() );
	}
}


/*
===============
Minimize the game so that user is back at the desktop
===============
*/
void VKimp_Minimize( void )
{
    if(r_fullscreen->integer == 1)
    {
        r_fullscreen->integer = 0;
        ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");
        r_fullscreen->integer = 1;
    }

	SDL_MinimizeWindow( SDL_window );
}

/*
	if( r_fullscreen->modified )
	{
		int         fullscreen;
		qboolean    needToToggle;
		qboolean    sdlToggled = qfalse;

		// Find out the current state
		fullscreen = !!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN );

		if( r_fullscreen->integer && ri.Cvar_VariableIntegerValue( "in_nograb" ) )
		{
			ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
			ri.Cvar_Set( "r_fullscreen", "0" );
			r_fullscreen->modified = qfalse;
		}

		// Is the state we want different from the current state?
		needToToggle = !!r_fullscreen->integer != fullscreen;

		if( needToToggle )
		{
			sdlToggled = SDL_SetWindowFullscreen( SDL_window, r_fullscreen->integer ) >= 0;

			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");

			ri.IN_Restart( );
		}

		r_fullscreen->modified = qfalse;
	}
*/
