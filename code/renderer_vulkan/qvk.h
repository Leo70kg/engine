#ifndef QVK_H_
#define QVK_H_

#include "VKimpl.h"


#define MAX_SWAPCHAIN_IMAGES    8

#define MAX_VK_PIPELINES        1024
#define MAX_VK_IMAGES           2048 // should be the same as MAX_DRAWIMAGES

#define IMAGE_CHUNK_SIZE        (32 * 1024 * 1024)



#define VERTEX_CHUNK_SIZE   (512 * 1024)

#define XYZ_SIZE            (4 * VERTEX_CHUNK_SIZE)
#define COLOR_SIZE          (1 * VERTEX_CHUNK_SIZE)
#define ST0_SIZE            (2 * VERTEX_CHUNK_SIZE)
#define ST1_SIZE            (2 * VERTEX_CHUNK_SIZE)

#define XYZ_OFFSET          0
#define COLOR_OFFSET        (XYZ_OFFSET + XYZ_SIZE)
#define ST0_OFFSET          (COLOR_OFFSET + COLOR_SIZE)
#define ST1_OFFSET          (ST0_OFFSET + ST0_SIZE)

#define VERTEX_BUFFER_SIZE  (XYZ_SIZE + COLOR_SIZE + ST0_SIZE + ST1_SIZE)
#define INDEX_BUFFER_SIZE   (2 * 1024 * 1024)

const char * cvtResToStr(VkResult result);


#define VK_CHECK(function_call) { \
	VkResult result = function_call; \
	if (result<0) \
		ri.Error(ERR_FATAL, \
        "Vulkan: error %s returned by %s", cvtResToStr(result), #function_call); \
}


enum Vk_Shader_Type {
	single_texture,
	multi_texture_mul,
	multi_texture_add
};

// used with cg_shadows == 2
enum Vk_Shadow_Phase {
	disabled,
	shadow_edges_rendering,
	fullscreen_quad_rendering
};

enum Vk_Depth_Range {
	normal, // [0..1]
	force_zero, // [0..0]
	force_one, // [1..1]
	weapon // [0..0.3]
};


struct Vk_Pipeline_Def {
	enum Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	int face_culling;// cullType_t
	VkBool32 polygon_offset;
	VkBool32 clipping_plane;
	VkBool32 mirror;
	VkBool32 line_primitives;
	enum Vk_Shadow_Phase shadow_phase;
};



// Most of the renderer's code uses Vulkan API via function provides in this file but 
// there are few places outside of vk.cpp where we use Vulkan commands directly.




extern PFN_vkFreeDescriptorSets qvkFreeDescriptorSets;
extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
extern PFN_vkDestroyImageView qvkDestroyImageView;
extern PFN_vkDeviceWaitIdle qvkDeviceWaitIdle;


extern PFN_vkGetInstanceProcAddr						qvkGetInstanceProcAddr;

extern PFN_vkCreateInstance							    qvkCreateInstance;
extern PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

extern PFN_vkCreateDevice								qvkCreateDevice;
extern PFN_vkDestroyInstance							qvkDestroyInstance;
extern PFN_vkEnumerateDeviceExtensionProperties	    	qvkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
extern PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
extern PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties	    qvkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR		    qvkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR		    qvkGetPhysicalDeviceSurfaceSupportKHR;

#ifndef NDEBUG
extern PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
extern PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif

extern PFN_vkAllocateCommandBuffers					    qvkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets					    qvkAllocateDescriptorSets;
extern PFN_vkAllocateMemory							    qvkAllocateMemory;
extern PFN_vkBeginCommandBuffer					    	qvkBeginCommandBuffer;
extern PFN_vkBindBufferMemory							qvkBindBufferMemory;
extern PFN_vkBindImageMemory							qvkBindImageMemory;
extern PFN_vkCmdBeginRenderPass					    	qvkCmdBeginRenderPass;
extern PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer					    	qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
extern PFN_vkCmdBlitImage								qvkCmdBlitImage;
extern PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
extern PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage								qvkCmdCopyImage;
extern PFN_vkCmdDraw									qvkCmdDraw;
extern PFN_vkCmdDrawIndexed						    	qvkCmdDrawIndexed;
extern PFN_vkCmdEndRenderPass							qvkCmdEndRenderPass;
extern PFN_vkCmdPipelineBarrier					    	qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants							qvkCmdPushConstants;
extern PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
extern PFN_vkCmdSetScissor								qvkCmdSetScissor;
extern PFN_vkCmdSetViewport						    	qvkCmdSetViewport;
extern PFN_vkCreateBuffer								qvkCreateBuffer;
extern PFN_vkCreateCommandPool							qvkCreateCommandPool;
extern PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
extern PFN_vkCreateFence								qvkCreateFence;
extern PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
extern PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
extern PFN_vkCreateImage								qvkCreateImage;
extern PFN_vkCreateImageView							qvkCreateImageView;
extern PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
extern PFN_vkCreateRenderPass							qvkCreateRenderPass;
extern PFN_vkCreateSampler								qvkCreateSampler;
extern PFN_vkCreateSemaphore							qvkCreateSemaphore;
extern PFN_vkCreateShaderModule				    		qvkCreateShaderModule;
extern PFN_vkDestroyBuffer								qvkDestroyBuffer;
extern PFN_vkDestroyCommandPool					    	qvkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout			    	qvkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice								qvkDestroyDevice;
extern PFN_vkDestroyFence								qvkDestroyFence;
extern PFN_vkDestroyFramebuffer					    	qvkDestroyFramebuffer;
extern PFN_vkDestroyImage								qvkDestroyImage;
extern PFN_vkDestroyImageView							qvkDestroyImageView;
extern PFN_vkDestroyPipeline							qvkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
extern PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
extern PFN_vkDestroySampler						    	qvkDestroySampler;
extern PFN_vkDestroySemaphore							qvkDestroySemaphore;
extern PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle						    	qvkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
extern PFN_vkFreeCommandBuffers					    	qvkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets					    	qvkFreeDescriptorSets;
extern PFN_vkFreeMemory							    	qvkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceQueue						    	qvkGetDeviceQueue;
extern PFN_vkGetImageMemoryRequirements			    	qvkGetImageMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
extern PFN_vkMapMemory									qvkMapMemory;
extern PFN_vkQueueSubmit								qvkQueueSubmit;
extern PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
extern PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
extern PFN_vkResetFences								qvkResetFences;
extern PFN_vkUpdateDescriptorSets						qvkUpdateDescriptorSets;
extern PFN_vkWaitForFences								qvkWaitForFences;
extern PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
extern PFN_vkCreateSwapchainKHR				    		qvkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
extern PFN_vkQueuePresentKHR							qvkQueuePresentKHR;



//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(void);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown(void);




//void vk_create_instance(void);
//void vk_create_device(void);
VkPipeline vk_create_pipeline(const struct Vk_Pipeline_Def* def);
void vk_bind_geometry(void);
void vk_clear_attachments(VkBool32 clear_depth_stencil, VkBool32 clear_color, float* color);

VkRect2D get_scissor_rect(void);


void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access);

void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkAccessFlags src_access_flags, VkImageLayout old_layout, VkAccessFlags dst_access_flags, VkImageLayout new_layout);

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties);
//
// Resources allocation.
//
VkPipeline vk_find_pipeline(const struct Vk_Pipeline_Def* def);
void VK_GetProcAddress(void);
void VK_ClearProcAddress(void);

//
// Rendering setup.
//
void vk_shade_geometry(VkPipeline pipeline, VkBool32 multitexture, enum Vk_Depth_Range depth_range, VkBool32 indexed);
void vk_begin_frame(void);
void vk_end_frame(void);

void vk_read_pixels(unsigned char* buffer); // screenshots

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
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	uint32_t swapchain_image_index;

	VkSemaphore image_acquired ;
	VkSemaphore rendering_finished;
	VkFence rendering_finished_fence;

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

	//
	// Shader modules.
	//
	VkShaderModule single_texture_vs;
	VkShaderModule single_texture_clipping_plane_vs;
	VkShaderModule single_texture_fs;
	VkShaderModule multi_texture_vs;
	VkShaderModule multi_texture_clipping_plane_vs;
	VkShaderModule multi_texture_mul_fs;
	VkShaderModule multi_texture_add_fs;

	//
	// Standard pipelines.
	//
	VkPipeline skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	VkPipeline shadow_volume_pipelines[2][2];
	VkPipeline shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	VkPipeline fog_pipelines[2][3][2];

	// dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	VkPipeline dlight_pipelines[2][3][2];

	// debug visualization pipelines
	VkPipeline tris_debug_pipeline;
	VkPipeline tris_mirror_debug_pipeline;
	VkPipeline normals_debug_pipeline;
	VkPipeline surface_debug_pipeline_solid;
	VkPipeline surface_debug_pipeline_outline;
	VkPipeline images_debug_pipeline;
};



struct Vk_Image {
	VkImage handle;
	VkImageView view;

	// Descriptor set that contains single descriptor used to access the given image.
	// It is updated only once during image initialization.
	VkDescriptorSet descriptor_set;
};

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
struct Vk_World {
	//
	// Resources.
	//
	int num_pipelines;
	struct Vk_Pipeline_Def pipeline_defs[MAX_VK_PIPELINES];
	VkPipeline pipelines[MAX_VK_PIPELINES];
	float pipeline_create_time;

	struct Vk_Image images[MAX_VK_IMAGES];

	//
	// State.
	//

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment == true), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == false).
	VkBool32 dirty_depth_attachment;

	float modelview_transform[16];
};


extern struct Vk_Instance vk;

#endif
