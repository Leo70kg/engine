#ifndef VK_INSTANCE_H_
#define VK_INSTANCE_H_


#define MAX_SWAPCHAIN_IMAGES    8


// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
	VkInstance instance ;
	VkPhysicalDevice physical_device;
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surface_format;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count ;
	VkImage swapchain_images_array[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	uint32_t swapchain_image_index;


	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;

	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;

	VkRenderPass render_pass;
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;

	VkBuffer vertex_buffer;
	unsigned char* vertex_buffer_ptr ; // pointer to mapped vertex buffer
	int xyz_elements;
	int color_st_elements;

	VkBuffer index_buffer;
	unsigned char* index_buffer_ptr; // pointer to mapped index buffer
	VkDeviceSize index_buffer_offset;

	// host visible memory that holds both vertex and index data
	VkDeviceMemory geometry_buffer_memory;


};



extern struct Vk_Instance vk;



#endif
