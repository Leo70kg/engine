#ifndef VKIMPL_H_
#define VKIMPL_H_


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/


#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"


void VKimp_Init(void);
void VKimp_Shutdown(void);
void VKimp_CreateSurface(void);
void VKimp_CreateInstance(void);
void VKimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
void VKimp_Minimize( void );


#endif
