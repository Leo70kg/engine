#ifndef VK_CLEAER_ATTACHMENTS_H_
#define VK_CLEAER_ATTACHMENTS_H_


#include "VKimpl.h"

//void vk_clear_attachments(VkBool32 clear_depth_stencil, VkBool32 clear_color, float* color);

void vk_clearColorAttachments(const float* color);
void vk_clearDepthStencilAttachments(void);
void set_depth_attachment(VkBool32 s);

#endif
