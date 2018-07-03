#ifndef GLIMP_H_
#define GLIMP_H_


#include "../sys/sys_local.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
////////////////////////////////////////////////////////////////

#ifdef _WIN32
	#include "../SDL2/include/SDL.h"
#else
	#include <SDL2/SDL.h>
#endif



void GLimp_Init(glconfig_t *glConfig, qboolean coreContext);
void GLimp_Shutdown(void);
void GLimp_EndFrame(void);

void GLimp_LogComment(char *comment);
void GLimp_Minimize(void);

void GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

void GLimp_DeleteGLContext(void);
void GLimp_DestroyWindow(void);

void* GLimp_GetProcAddress(const char* fun);
qboolean GLimp_ExtensionSupported(const char* fun);


void IN_Init(void);
void IN_Frame(void);
void IN_Shutdown(void);
#endif
