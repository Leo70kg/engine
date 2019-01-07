#ifndef VK_FRAME_H_
#define VK_FRAME_H_

void vk_begin_frame(void);
void vk_end_frame(void);


void vk_createFrameBuffers(void);
void vk_destroyFrameBuffers(void);

void vk_createSwapChain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format);

void vk_create_sync_primitives(void);
void vk_destroy_sync_primitives(void);
#endif
