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
#include "glimp.h"


typedef enum
{
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;

static cvar_t* r_fullscreen;
static cvar_t* r_availableModes;


SDL_Window *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;


static SDL_DisplayMode sdlDispMode;


static cvar_t* r_allowResize; // make window resizable
static cvar_t* r_customwidth;
static cvar_t* r_customheight;
static cvar_t* r_customPixelAspect;
static cvar_t* r_mode;
static cvar_t* r_stencilbits;
static cvar_t* r_colorbits;
static cvar_t* r_depthbits;

static cvar_t* r_displayIndex;
static cvar_t* r_displayRefresh;


// not used cvar, keep it for backward compatibility
static cvar_t* r_stereoSeparation;



typedef struct vidmode_s {
	const char *description;
	int width, height;
	float pixelAspect;		// pixel width / height
} vidmode_t;

static const vidmode_t r_vidModes[] = {
	{ "Mode  0: 320x240",		320,	240,	1 },
	{ "Mode  1: 400x300",		400,	300,	1 },
	{ "Mode  2: 512x384",		512,	384,	1 },
	{ "Mode  3: 640x480 (480p)",	640,	480,	1 },
	{ "Mode  4: 800x600",		800,	600,	1 },
	{ "Mode  5: 960x720",		960,	720,	1 },
	{ "Mode  6: 1024x768",		1024,	768,	1 },
	{ "Mode  7: 1152x864",		1152,	864,	1 },
	{ "Mode  8: 1280x1024",		1280,	1024,	1 },
	{ "Mode  9: 1600x1200",		1600,	1200,	1 },
	{ "Mode 10: 2048x1536",		2048,	1536,	1 },
	{ "Mode 11: 856x480",		856,	480,	1 },		// Q3 MODES END HERE AND EXTENDED MODES BEGIN
	{ "Mode 12: 1280x720 (720p)",	1280,	720,	1 },
	{ "Mode 13: 1280x768",		1280,	768,	1 },
	{ "Mode 14: 1280x800",		1280,	800,	1 },
	{ "Mode 15: 1280x960",		1280,	960,	1 },
	{ "Mode 16: 1360x768",		1360,	768,	1 },
	{ "Mode 17: 1366x768",		1366,	768,	1 }, // yes there are some out there on that extra 6
	{ "Mode 18: 1360x1024",		1360,	1024,	1 },
	{ "Mode 19: 1400x1050",		1400,	1050,	1 },
	{ "Mode 20: 1400x900",		1400,	900,	1 },
	{ "Mode 21: 1600x900",		1600,	900,	1 },
	{ "Mode 22: 1680x1050",		1680,	1050,	1 },
	{ "Mode 23: 1920x1080 (1080p)",	1920,	1080,	1 },
	{ "Mode 24: 1920x1200",		1920,	1200,	1 },
	{ "Mode 25: 1920x1440",		1920,	1440,	1 },
	{ "Mode 26: 2560x1600",		2560,	1600,	1 },
	{ "Mode 27: 3840x2160 (4K)",	3840,	2160,	1 }
};
static const int s_numVidModes = ARRAY_LEN( r_vidModes );

static void R_ModeList_f( void )
{
	int i;

	Com_Printf( "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
    {
		Com_Printf("%s\n", r_vidModes[i].description );
	}
	Com_Printf( "\n" );
}





/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	IN_Shutdown();

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


void GLimp_LogComment( char *comment )
{
}

void GLimp_DeleteGLContext(void)
{
    SDL_GL_DeleteContext( SDL_glContext );
    SDL_glContext = NULL;
}

void GLimp_DestroyWindow(void)
{
    SDL_DestroyWindow( SDL_window );
    SDL_window = NULL;

    Cmd_RemoveCommand("modelist");
}


void* GLimp_GetProcAddress(const char* fun)
{
    return SDL_GL_GetProcAddress(fun);
}


qboolean GLimp_ExtensionSupported(const char* fun)
{
	if(SDL_GL_ExtensionSupported(fun))
	{
		return qtrue;
	}

    return qfalse;
}


static void GLimp_DetectAvailableModes(void)
{
	int i, j;
	char buf[ MAX_STRING_CHARS ] = { 0 };


	SDL_DisplayMode windowMode;
	int display = SDL_GetWindowDisplayIndex( SDL_window );
	if( display < 0 )
	{
		Com_Printf("Couldn't get window display index, no resolutions detected: %s\n", SDL_GetError() );
		return;
	}
	int numSDLModes = SDL_GetNumDisplayModes( display );

	if( SDL_GetWindowDisplayMode( SDL_window, &windowMode ) < 0 || numSDLModes <= 0 )
	{
		Com_Printf("Couldn't get window display mode, no resolutions detected: %s\n", SDL_GetError() );
		return;
	}

	int numModes = 0;
	SDL_Rect* modes = SDL_calloc(numSDLModes, sizeof( SDL_Rect ));
	if ( !modes )
	{
        ////////////////////////////////////
		Com_Error(ERR_FATAL, "Out of memory" );
        ////////////////////////////////////
	}

	for( i = 0; i < numSDLModes; i++ )
	{
		SDL_DisplayMode mode;

		if( SDL_GetDisplayMode( display, i, &mode ) < 0 )
			continue;

		if( !mode.w || !mode.h )
		{
			Com_Printf( "Display supports any resolution\n" );
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
			Com_Printf( "Skipping mode %ux%u, buffer too small\n", modes[ i ].w, modes[ i ].h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		Com_Printf("Available modes: '%s'\n", buf );
		Cvar_Set( "r_availableModes", buf );
	}
	SDL_free( modes );
}


/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen, glconfig_t *glConfig, qboolean coreContext)
{
	static int display_in_use = 0; /* Only using first display */
	int samples = 0;
	SDL_Surface *icon = NULL;
	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
	SDL_DisplayMode desktopMode;
	int display = 0;
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	Com_Printf( "Initializing OpenGL display\n");

	if ( r_allowResize->integer )
		flags |= SDL_WINDOW_RESIZABLE;

#ifdef USE_ICON
	icon = SDL_CreateRGBSurfaceFrom(
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

	//Let SDL_GetDisplayMode handle this
	SDL_Init(SDL_INIT_VIDEO);
	Com_Printf("SDL_GetNumVideoDisplays(): %d\n", SDL_GetNumVideoDisplays());
	
	int display_mode_count = SDL_GetNumDisplayModes(display_in_use);
	if (display_mode_count < 1)
	{
		Com_Printf("SDL_GetNumDisplayModes failed: %s", SDL_GetError());
	}
	//mode = 0: use the first display mode SDL return;
    if (SDL_GetDisplayMode(display_in_use, 0, &sdlDispMode) != 0)
	{
    	Com_Printf("SDL_GetDisplayMode failed: %s", SDL_GetError());
	}

	Uint32 f = sdlDispMode.format;

	glConfig->vidWidth = sdlDispMode.w;
	glConfig->vidHeight = sdlDispMode.h;
	glConfig->windowAspect = (float)glConfig->vidWidth / (float)glConfig->vidHeight;
	glConfig->refresh_rate = sdlDispMode.refresh_rate;

    Com_Printf("Mode %i\tbpp %i\t%s\t%i x %i", mode, SDL_BITSPERPIXEL(f), SDL_GetPixelFormatName(f), sdlDispMode.w, sdlDispMode.h);

	// If a window exists, note its display index
	if( SDL_window != NULL )
	{
		display = SDL_GetWindowDisplayIndex( SDL_window );
		if( display < 0 )
		{
			Com_Printf("SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
		}
	}



	if (mode == -2)
	{
		int tmp = SDL_GetDesktopDisplayMode(display, &desktopMode);
		Com_Printf("Display aspect: %.3f\n", glConfig->windowAspect);
		// use desktop video resolution
		if( (tmp == 0) && (desktopMode.h > 0) )
		{
			glConfig->vidWidth = desktopMode.w;
			glConfig->vidHeight = desktopMode.h;
			glConfig->windowAspect = (float)desktopMode.w / (float)desktopMode.h;
        	glConfig->refresh_rate = desktopMode.refresh_rate;
		}
		else
		{
			glConfig->vidWidth = 640;
			glConfig->vidHeight = 480;
			glConfig->windowAspect = (float)glConfig->vidWidth / (float)glConfig->vidHeight;
			Com_Printf( "Cannot determine display aspect, assuming 1.333\n" );
		}
	}
	else if(mode == -1)
	{
		// custom. 
		glConfig->vidWidth = r_customwidth->integer;
		glConfig->vidHeight = r_customheight->integer;
		glConfig->windowAspect = glConfig->vidWidth / (float)glConfig->vidHeight;
        glConfig->refresh_rate = r_displayRefresh->integer;

	}
	else if(mode > 0)
	{
        SDL_GetDisplayMode(display_in_use, mode, &sdlDispMode);
        
        glConfig->vidWidth = sdlDispMode.w;
        glConfig->vidHeight = sdlDispMode.h;
        glConfig->windowAspect = (float)glConfig->vidWidth / (float)glConfig->vidHeight;
        glConfig->refresh_rate = sdlDispMode.refresh_rate;
    }

	// Center window
	if(!fullscreen)
	{
		x = ( desktopMode.w / 2 ) - ( glConfig->vidWidth / 2 );
		y = ( desktopMode.h / 2 ) - ( glConfig->vidHeight / 2 );
	}

	// Destroy existing state if it exists
	if( SDL_glContext != NULL )
	{
		SDL_GL_DeleteContext( SDL_glContext );
		SDL_glContext = NULL;
	}

	if( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		Com_Printf( "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if( fullscreen )
	{
		flags |= SDL_WINDOW_FULLSCREEN;
		flags |= SDL_WINDOW_BORDERLESS;
		glConfig->isFullscreen = qtrue;
	}
	else
	{
		glConfig->isFullscreen = qfalse;
	}


	int realColorBits[3];
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 0 );

	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

    SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
	
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );


	SDL_window = SDL_CreateWindow( CLIENT_WINDOW_TITLE, x, y,
						glConfig->vidWidth, glConfig->vidHeight, flags );
	if( SDL_window == NULL )
	{
		Com_Error(ERR_FATAL,"SDL_CreateWindow failed: %s\n", SDL_GetError( ) );
	}

	SDL_SetWindowIcon( SDL_window, icon );


	if (coreContext)
	{
		int profileMask, majorVersion, minorVersion;
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profileMask);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &majorVersion);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minorVersion);

		Com_Printf("Trying to get an OpenGL 3.2 core context\n");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

        SDL_glContext = SDL_GL_CreateContext(SDL_window);

		if (SDL_glContext == NULL)
		{
			Com_Printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
			Com_Printf("Reverting to default context\n");

			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profileMask);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minorVersion);
		}
		else
		{
			Com_Printf("SDL_GL_CreateContext succeeded.\n");
		}
	}
	else
	{
        SDL_glContext = SDL_GL_CreateContext( SDL_window );
		if( SDL_glContext == NULL )
		{
			Com_Printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError( ) );
		}
	}

    #define SWAP_INTERVAL   0
	if( SDL_GL_SetSwapInterval( SWAP_INTERVAL ) == -1 )
	{
		Com_Printf("SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError( ) );
	}

	SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &realColorBits[0] );
	SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &realColorBits[1] );
	SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &realColorBits[2] );
	SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &glConfig->depthBits );
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &glConfig->stencilBits );

	glConfig->colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];

	Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n",
			glConfig->colorBits, glConfig->depthBits, glConfig->stencilBits );



	SDL_FreeSurface( icon );

	if( !SDL_window )
	{
		Com_Printf("Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	GLimp_DetectAvailableModes();


	return RSERR_OK;
}


static qboolean GLimp_CreateWindowAndSetMode(int mode, qboolean fullscreen, glconfig_t *glConfig, qboolean gl3Core)
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		const char *driverName;

		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			Com_Printf("SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}

		driverName = SDL_GetCurrentVideoDriver( );
		Com_Printf(" SDL using driver \"%s\"\n", driverName );
	}
 
	err = GLimp_SetMode(mode, fullscreen, glConfig ,gl3Core);
	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Printf("...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			Com_Printf("...WARNING: could not set the given mode \n");
			return qfalse;
		default:
			break;
	}

	return qtrue;
}


#define R_MODE_FALLBACK 3 // 640 * 480

/*
 * This routine is responsible for initializing the OS specific portions of OpenGL
 */
void GLimp_Init(glconfig_t *glConfig, qboolean coreContext)
{
    Com_Printf("\n-------- Glimp_Init() started! --------\n");
    const char *fsstrings[] = {
		"windowed",
		"fullscreen"
	};

	r_allowResize = Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH );

	r_mode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH ); // leilei - -2 is so convenient for modern day PCs
	

	r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE );
	r_customwidth = Cvar_Get( "r_customwidth", "1920", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customheight", "1080", CVAR_ARCHIVE | CVAR_LATCH );
	r_customPixelAspect = Cvar_Get( "r_customPixelAspect", "1.78", CVAR_ARCHIVE | CVAR_LATCH );

	r_stencilbits = Cvar_Get( "r_stencilbits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorbits = Cvar_Get( "r_colorbits", "24", CVAR_ARCHIVE | CVAR_LATCH );
	r_depthbits = Cvar_Get( "r_depthbits", "24", CVAR_ARCHIVE | CVAR_LATCH );

	r_displayIndex = Cvar_Get( "r_displayIndex", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_displayRefresh = Cvar_Get( "r_displayRefresh", "60", CVAR_LATCH );
	Cvar_CheckRange( r_displayRefresh, 0, 200, qtrue );

	r_stereoSeparation = Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE );

	if( Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		Cvar_Set( "r_mode", "3"); // R_MODE_FALLBACK
		Cvar_Set( "r_fullscreen", "0" );
		Cvar_Set( "com_abnormalExit", "0" );
	}


	// Create the window and set up the context
	if(GLimp_CreateWindowAndSetMode(r_mode->integer, r_fullscreen->integer, glConfig, coreContext))
		goto success;


	// Finally, try the default screen resolution
	if( r_mode->integer != R_MODE_FALLBACK )
	{
		Com_Printf("Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, R_MODE_FALLBACK );

		if(GLimp_CreateWindowAndSetMode(R_MODE_FALLBACK, qfalse, glConfig, coreContext))
			goto success;
	}

	// Nothing worked, give up
	Com_Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );

success:

	Com_Printf( "MODE: %s, %d x %d, refresh rate: %dhz\n", fsstrings[r_fullscreen->integer == 1], glConfig->vidWidth, glConfig->vidHeight, glConfig->refresh_rate);


	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	r_availableModes = Cvar_Get("r_availableModes", "", CVAR_ROM);
    
    Cmd_AddCommand( "modelist", R_ModeList_f );
    
    Com_Printf("\n-------- Glimp_Init() finished! --------\n");
}


/*
 * GLimp_EndFrame() Responsible for doing a swapbuffers
 */
void GLimp_EndFrame( void )
{

	SDL_GL_SwapWindow( SDL_window );


	if( r_fullscreen->modified )
	{
		qboolean sdlToggled = qfalse;

		// Find out the current state
		int fullscreen = !!( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_FULLSCREEN );

		// Is the state we want different from the current state?
		qboolean needToToggle = !!r_fullscreen->integer != fullscreen;

		if( needToToggle )
		{
			sdlToggled = SDL_SetWindowFullscreen( SDL_window, r_fullscreen->integer ) >= 0;

			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				Cbuf_ExecuteText(EXEC_APPEND, "vid_restart\n");

			IN_Init();
		}

		r_fullscreen->modified = qfalse;
	}
}

