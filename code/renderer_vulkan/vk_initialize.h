#ifndef VK_INITIALLIZE_H_
#define VK_INITIALLIZE_H_



// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(void);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown(void);

void vk_release_resources(void);



#endif
