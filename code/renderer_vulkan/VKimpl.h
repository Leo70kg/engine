#ifndef VKIMPL_H_
#define VKIMPL_H_


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/


#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

void vk_createWindow(void);
void vk_destroyWindow(void);

void vk_getInstanceProcAddrImpl(void);

void VKimp_CreateSurface(void);
void VKimp_CreateInstance(void);

void VKimp_Minimize( void );


#endif
