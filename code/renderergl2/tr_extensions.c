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
#include "tr_dsa.h"


cvar_t* r_ext_compressed_textures;
static cvar_t* r_ext_texture_filter_anisotropic;
static cvar_t* r_ext_max_anisotropy;
static int qglMajorVersion, qglMinorVersion;
//int qglesMajorVersion, qglesMinorVersion;

#define GLE(ret, name, ...) name##proc * qgl##name;
QGL_1_1_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
QGL_2_0_PROCS;
QGL_ARB_framebuffer_object_PROCS;
QGL_ARB_vertex_array_object_PROCS;
QGL_EXT_direct_state_access_PROCS;
QGL_3_0_PROCS;
#undef GLE


void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);



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
		ri.Error(ERR_FATAL, "ERROR: Missing OpenGL function %s\n", "gl" #name ); \
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

	if ( QGL_VERSION_ATLEAST( 1, 2 ) ) {
		QGL_1_1_PROCS;
		QGL_DESKTOP_1_1_PROCS;
	}else {
		ri.Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
	}

	if ( QGL_VERSION_ATLEAST( 3, 0 ) ) {
		QGL_3_0_PROCS;
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
	//qglesMajorVersion = 0;
	//qglesMinorVersion = 0;

	QGL_1_1_PROCS;
	QGL_DESKTOP_1_1_PROCS;
	//QGL_ES_1_1_PROCS;
	QGL_3_0_PROCS;

#undef GLE
}





void GLimp_InitExtensions( glconfig_t *glConfig )
{
    r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "2", CVAR_ARCHIVE | CVAR_LATCH );
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


void GLimp_InitExtraExtensions()
{

	char *extension;
	const char* result[3] = { "...ignoring %s\n", "...using %s\n", "...%s not found\n" };
	qboolean q_gl_version_at_least_3_0;
	qboolean q_gl_version_at_least_3_2;

	// Check OpenGL version
	if ( !QGL_VERSION_ATLEAST( 2, 0 ) )
		ri.Error(ERR_FATAL, "OpenGL 2.0 required!");
	ri.Printf(PRINT_ALL, "...using OpenGL %s\n", glConfig.version_string);

	q_gl_version_at_least_3_0 = QGL_VERSION_ATLEAST( 3, 0 );
	q_gl_version_at_least_3_2 = QGL_VERSION_ATLEAST( 3, 2 );

	// Check if we need Intel graphics specific fixes.
	glRefConfig.intelGraphics = qfalse;
	if (strstr((char *)qglGetString(GL_RENDERER), "Intel"))
		glRefConfig.intelGraphics = qtrue;

	// set DSA fallbacks
#define GLE(ret, name, ...) qgl##name = GLDSA_##name;
	QGL_EXT_direct_state_access_PROCS;
#undef GLE

	// GL function loader, based on https://gist.github.com/rygorous/16796a0c876cf8a5f542caddb55bce8a
#define GLE(ret, name, ...) qgl##name = (name##proc *) ri.GLimpGetProcAddress("gl" #name);

	// OpenGL 1.3, was GL_ARB_texture_compression
	QGL_1_3_PROCS;

	// OpenGL 1.5, was GL_ARB_vertex_buffer_object and GL_ARB_occlusion_query
	QGL_1_5_PROCS;
	glRefConfig.occlusionQuery = qtrue;

	// OpenGL 2.0, was GL_ARB_shading_language_100, GL_ARB_vertex_program, GL_ARB_shader_objects, and GL_ARB_vertex_shader
	QGL_2_0_PROCS;




///////

	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE | CVAR_LATCH );

	glConfig.textureCompression = TC_NONE;

	// GL_EXT_texture_compression_s3tc
    if( ri.GLimpExtensionSupported( "GL_ARB_texture_compression" ) && ri.GLimpExtensionSupported( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
	if(glConfig.textureCompression == TC_NONE)
	{
		if( ri.GLimpExtensionSupported( "GL_S3_s3tc" ) )
		{
			if ( r_ext_compressed_textures->value )
			{
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
			}
			else
			{
				ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
		}
	}

///////

	// OpenGL 3.0 - GL_ARB_framebuffer_object
	extension = "GL_ARB_framebuffer_object";
	glRefConfig.framebufferObject = qfalse;
	glRefConfig.framebufferBlit = qfalse;
	glRefConfig.framebufferMultisample = qfalse;
	if (q_gl_version_at_least_3_0 || ri.GLimpExtensionSupported(extension))
	{
		glRefConfig.framebufferObject = !!r_ext_framebuffer_object->integer;
		glRefConfig.framebufferBlit = qtrue;
		glRefConfig.framebufferMultisample = qtrue;

		qglGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &glRefConfig.maxRenderbufferSize);
		qglGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &glRefConfig.maxColorAttachments);

		QGL_ARB_framebuffer_object_PROCS;

		ri.Printf(PRINT_ALL, result[glRefConfig.framebufferObject], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.0 - GL_ARB_vertex_array_object
	extension = "GL_ARB_vertex_array_object";
	glRefConfig.vertexArrayObject = qfalse;
	if (q_gl_version_at_least_3_0 || ri.GLimpExtensionSupported(extension))
	{
		if (q_gl_version_at_least_3_0)
		{
			// force VAO, core context requires it
			glRefConfig.vertexArrayObject = qtrue;
		}
		else
		{
			glRefConfig.vertexArrayObject = !!r_arb_vertex_array_object->integer;
		}

		QGL_ARB_vertex_array_object_PROCS;

		ri.Printf(PRINT_ALL, result[glRefConfig.vertexArrayObject], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.0 - GL_ARB_texture_float
	extension = "GL_ARB_texture_float";
	glRefConfig.textureFloat = qfalse;
	if (q_gl_version_at_least_3_0 || ri.GLimpExtensionSupported(extension))
	{
		glRefConfig.textureFloat = !!r_ext_texture_float->integer;

		ri.Printf(PRINT_ALL, result[glRefConfig.textureFloat], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.2 - GL_ARB_depth_clamp
	extension = "GL_ARB_depth_clamp";
	glRefConfig.depthClamp = qfalse;
	if (q_gl_version_at_least_3_2 || ri.GLimpExtensionSupported(extension))
	{
		glRefConfig.depthClamp = qtrue;

		ri.Printf(PRINT_ALL, result[glRefConfig.depthClamp], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// OpenGL 3.2 - GL_ARB_seamless_cube_map
	extension = "GL_ARB_seamless_cube_map";
	glRefConfig.seamlessCubeMap = qfalse;
	if (q_gl_version_at_least_3_2 || ri.GLimpExtensionSupported(extension))
	{
		glRefConfig.seamlessCubeMap = !!r_arb_seamless_cube_map->integer;

		ri.Printf(PRINT_ALL, result[glRefConfig.seamlessCubeMap], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// Determine GLSL version
	if (1)
	{
		char version[256];

		Q_strncpyz(version, (char *)qglGetString(GL_SHADING_LANGUAGE_VERSION), sizeof(version));

		sscanf(version, "%d.%d", &glRefConfig.glslMajorVersion, &glRefConfig.glslMinorVersion);

		ri.Printf(PRINT_ALL, "...using GLSL version %s\n", version);
	}

	glRefConfig.memInfo = MI_NONE;

	// GL_NVX_gpu_memory_info
	extension = "GL_NVX_gpu_memory_info";
	if( ri.GLimpExtensionSupported( extension ) )
	{
		glRefConfig.memInfo = MI_NVX;

		ri.Printf(PRINT_ALL, result[1], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// GL_ATI_meminfo
	extension = "GL_ATI_meminfo";
	if( ri.GLimpExtensionSupported( extension ) )
	{
		if (glRefConfig.memInfo == MI_NONE)
		{
			glRefConfig.memInfo = MI_ATI;

			ri.Printf(PRINT_ALL, result[1], extension);
		}
		else
		{
			ri.Printf(PRINT_ALL, result[0], extension);
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	glRefConfig.textureCompression = TCR_NONE;

	// GL_ARB_texture_compression_rgtc
	extension = "GL_ARB_texture_compression_rgtc";
	if (ri.GLimpExtensionSupported(extension))
	{
		qboolean useRgtc = r_ext_compressed_textures->integer >= 1;

		if (useRgtc)
			glRefConfig.textureCompression |= TCR_RGTC;

		ri.Printf(PRINT_ALL, result[useRgtc], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	glRefConfig.swizzleNormalmap = r_ext_compressed_textures->integer && !(glRefConfig.textureCompression & TCR_RGTC);

	// GL_ARB_texture_compression_bptc
	extension = "GL_ARB_texture_compression_bptc";
	if (ri.GLimpExtensionSupported(extension))
	{
		qboolean useBptc = r_ext_compressed_textures->integer >= 2;

		if (useBptc)
			glRefConfig.textureCompression |= TCR_BPTC;

		ri.Printf(PRINT_ALL, result[useBptc], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

	// GL_EXT_direct_state_access
	extension = "GL_EXT_direct_state_access";
	glRefConfig.directStateAccess = qfalse;
	if (ri.GLimpExtensionSupported(extension))
	{
		glRefConfig.directStateAccess = !!r_ext_direct_state_access->integer;

		// QGL_*_PROCS becomes several functions, do not remove {}
		if (glRefConfig.directStateAccess)
		{
			QGL_EXT_direct_state_access_PROCS;
		}

		ri.Printf(PRINT_ALL, result[glRefConfig.directStateAccess], extension);
	}
	else
	{
		ri.Printf(PRINT_ALL, result[2], extension);
	}

#undef GLE
}
