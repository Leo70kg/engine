/*
===========================================================================
Copyright (C) 2011 James Canete (use.less01@gmail.com)

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
// tr_extensions.c - extensions needed by the renderer not in sdl_glimp.c


#include "tr_local.h"

static cvar_t* r_ext_texture_filter_anisotropic;
static cvar_t* r_ext_max_anisotropy;
static int qglMajorVersion, qglMinorVersion;

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

#define GLE(ret, name, ...) name##proc * qgl##name;
QGL_1_1_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
#undef GLE


/*
===============
GLimp_GetProcAddresses

Get addresses for OpenGL functions.
===============
*/
qboolean GLimp_GetProcAddresses( void )
{
	qboolean success = qtrue;
	const char *version;

#ifdef __SDL_NOGETPROCADDR__
#define GLE( ret, name, ... ) qgl##name = gl#name;
#else
#define GLE( ret, name, ... ) qgl##name = (name##proc *) ri.GLimpGetProcAddress("gl" #name); \
	if ( qgl##name == NULL ) { \
		ri.Error(ERR_FATAL, "Missing OpenGL function %s\n", "gl" #name ); \
		success = qfalse; \
	}
#endif

	// OpenGL 1.0 and OpenGL ES 1.0
	GLE(const GLubyte *, GetString, GLenum name)

	if ( !qglGetString ) {
		ri.Error( ERR_FATAL, "glGetString is NULL" );
	}

	version = (const char *)qglGetString( GL_VERSION );

	if ( !version ) {
		ri.Error( ERR_FATAL, "GL_VERSION is NULL\n" );
	}

    sscanf( version, "%d.%d", &qglMajorVersion, &qglMinorVersion );

	if ( QGL_VERSION_ATLEAST( 1, 1 ) ) {
		QGL_1_1_PROCS;
		QGL_DESKTOP_1_1_PROCS;
	}
    else {
		ri.Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
	}
#undef GLE

	return success;
}


/*
===============
GLimp_ClearProcAddresses

Clear addresses for OpenGL functions.
===============
*/
void GLimp_ClearProcAddresses( void )
{
#define GLE( ret, name, ... ) qgl##name = NULL;
	qglMajorVersion = 0;
	qglMinorVersion = 0;

	QGL_1_1_PROCS;
	QGL_DESKTOP_1_1_PROCS;
#undef GLE
}


void GLimp_InitExtensions( glconfig_t *glConfig )
{
	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "2", CVAR_ARCHIVE | CVAR_LATCH );
    r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE | CVAR_LATCH );

	ri.Printf( PRINT_ALL,  "\n...Initializing OpenGL extensions\n" );


	// GL_EXT_texture_env_add
	glConfig->textureEnvAddAvailable = qfalse;
	if ( ri.GLimpExtensionSupported( "EXT_texture_env_add" ) )
	{
		if ( ri.GLimpExtensionSupported( "GL_EXT_texture_env_add" ) )
		{
			glConfig->textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			glConfig->textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( ri.GLimpExtensionSupported( "GL_ARB_multitexture" ) )
	{
		qglMultiTexCoord2fARB = ri.GLimpGetProcAddress( "glMultiTexCoord2fARB" );
		qglActiveTextureARB = ri.GLimpGetProcAddress( "glActiveTextureARB" );
		qglClientActiveTextureARB = ri.GLimpGetProcAddress( "glClientActiveTextureARB" );

		if ( qglActiveTextureARB )
		{
			GLint glint = 0;
			qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
			glConfig->numTextureUnits = (int) glint;
			if ( glConfig->numTextureUnits > 1 )
			{
				ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
			}
			else
			{
				qglMultiTexCoord2fARB = NULL;
				qglActiveTextureARB = NULL;
				qglClientActiveTextureARB = NULL;
				ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
			}
		}
	}
	else
	{
		ri.Printf( PRINT_ALL,  "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( ri.GLimpExtensionSupported( "GL_EXT_compiled_vertex_array" ) )
	{
		ri.Printf( PRINT_ALL,  "...using GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void ( APIENTRY * )( GLint, GLint ) ) ri.GLimpGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) ri.GLimpGetProcAddress( "glUnlockArraysEXT" );
		if (!qglLockArraysEXT || !qglUnlockArraysEXT)
		{
			Com_Error(ERR_FATAL, "bad getprocaddress");
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	if ( ri.GLimpExtensionSupported( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer )
        {
            int maxAnisotropy = 0;
            char target_string[4] = {0};
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&maxAnisotropy );
			if ( maxAnisotropy <= 0 ) {
				ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
				maxAnisotropy = 0;
			}
			else
			{
                sprintf(target_string, "%d", maxAnisotropy);
				ri.Printf( PRINT_ALL,  "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy );
                ri.Cvar_Set( "r_ext_max_anisotropy", target_string);

			}
		}
		else
		{
			ri.Printf( PRINT_ALL,  "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
}
