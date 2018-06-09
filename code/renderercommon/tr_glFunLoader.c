#include "tr_common.h"
#include "qgl.h"

int qglMajorVersion, qglMinorVersion;
int qglesMajorVersion, qglesMinorVersion;


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
#define GLE( ret, name, ... ) qgl##name = (name##proc *) SDL_GL_GetProcAddress("gl" #name); \
	if ( qgl##name == NULL ) { \
		SDL_Log("ERROR: Missing OpenGL function %s\n", "gl" #name ); \
		success = qfalse; \
	}
#endif

	// OpenGL 1.0 and OpenGL ES 1.0
	GLE(const GLubyte *, GetString, GLenum name)

	if ( !qglGetString ) {
		Com_Error( ERR_FATAL, "glGetString is NULL" );
	}

	version = (const char *)qglGetString( GL_VERSION );

	if ( !version ) {
		Com_Error( ERR_FATAL, "GL_VERSION is NULL\n" );
	}

	if ( Q_stricmpn( "OpenGL ES", version, 9 ) == 0 ) {
		char profile[6]; // ES, ES-CM, or ES-CL
		sscanf( version, "OpenGL %5s %d.%d", profile, &qglesMajorVersion, &qglesMinorVersion );
		// common lite profile (no floating point) is not supported
		if ( Q_stricmp( profile, "ES-CL" ) == 0 ) {
			qglesMajorVersion = 0;
			qglesMinorVersion = 0;
		}
	} else {
		sscanf( version, "%d.%d", &qglMajorVersion, &qglMinorVersion );
	}

	if ( QGL_VERSION_ATLEAST( 1, 1 ) ) {
		QGL_1_1_PROCS;
		QGL_DESKTOP_1_1_PROCS;
	} else if ( qglesMajorVersion == 1 && qglesMinorVersion >= 1 ) {
		// OpenGL ES 1.1 (2.0 is not backward compatible)
		QGL_1_1_PROCS;
		//QGL_ES_1_1_PROCS;
		// error so this doesn't segfault due to NULL desktop GL functions being used
		Com_Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
	} else {
		Com_Error( ERR_FATAL, "Unsupported OpenGL Version: %s\n", version );
	}

	if ( QGL_VERSION_ATLEAST( 3, 0 ) || QGLES_VERSION_ATLEAST( 3, 0 ) ) {
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
	qglesMajorVersion = 0;
	qglesMinorVersion = 0;

	QGL_1_1_PROCS;
	QGL_DESKTOP_1_1_PROCS;
	//QGL_ES_1_1_PROCS;
	QGL_3_0_PROCS;

#undef GLE
}


