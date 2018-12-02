#include "qvk.h"
#include "tr_local.h"

#include "vk_initialize.h"

#include "vk_image.h"

#include "vk_instance.h"
#include "vk_shade_geometry.h"
#include "vk_create_pipeline.h"
#include "vk_frame.h"


#ifndef NDEBUG

static VkDebugReportCallbackEXT debug_callback;


VKAPI_ATTR VkBool32 VKAPI_CALL fpDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
	int32_t message_code, const char* layer_prefix, const char* message, void* user_data)
{
	
#ifdef _WIN32
	OutputDebugString(message);
	OutputDebugString("\n");
	DebugBreak();
#else
    ri.Printf(PRINT_WARNING, "%s\n", message);

#endif
	return VK_FALSE;
}


static void createDebugCallback( void )
{
    
    VkDebugReportCallbackCreateInfoEXT desc;
    desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    desc.pNext = NULL;
    desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT;
    desc.pfnCallback = &fpDebugCallback;
    desc.pUserData = NULL;

    VK_CHECK(qvkCreateDebugReportCallbackEXT(vk.instance, &desc, NULL, &debug_callback));
}
#endif


static VkRenderPass create_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format)
{
	VkAttachmentDescription attachments[2];
	attachments[0].flags = 0;
	attachments[0].format = color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[1].flags = 0;
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_attachment_ref;
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref;
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass;
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 0;
	desc.pDependencies = NULL;

	VkRenderPass render_pass;
	VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &render_pass));
	return render_pass;
}


static VkSwapchainKHR create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format)
{
	VkSurfaceCapabilitiesKHR surface_caps;
	VK_CHECK(qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));

	VkExtent2D image_extent = surface_caps.currentExtent;
	if ( (image_extent.width == 0xffffffff) && (image_extent.height == 0xffffffff))
    {
		image_extent.width = MIN(surface_caps.maxImageExtent.width, MAX(surface_caps.minImageExtent.width, 640u));
		image_extent.height = MIN(surface_caps.maxImageExtent.height, MAX(surface_caps.minImageExtent.height, 480u));
	}

	// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
	if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
		ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

	// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
	if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
		ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");

	// determine present mode and swapchain image count
	uint32_t nPM, i;
	qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &nPM, NULL);
    //std::vector<VkPresentModeKHR> present_modes(nPM);
	
    VkPresentModeKHR *pPresentModes = 
        (VkPresentModeKHR *)malloc(nPM * sizeof(VkPresentModeKHR));

    qvkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, &nPM, pPresentModes);

	int mailbox_supported = 0;
	int immediate_supported = 0;

    for ( i = 0; i < nPM; i++)
    {
		if (pPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			mailbox_supported = 1;
		else if (pPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			immediate_supported = 1;
	}

    free(pPresentModes);


	VkPresentModeKHR present_mode;
	uint32_t image_count;
	if (mailbox_supported)
    {
		present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		image_count = MAX(3u, surface_caps.minImageCount);
	}
    else
    {
		present_mode = immediate_supported ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX(2u, surface_caps.minImageCount);
	}

	if (surface_caps.maxImageCount > 0) {
		image_count = MIN(image_count, surface_caps.maxImageCount);
	}

	// create swap chain
	VkSwapchainCreateInfoKHR desc;
	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	VK_CHECK(qvkCreateSwapchainKHR(device, &desc, NULL, &swapchain));
	return swapchain;
}


static VkFormat get_depth_format(VkPhysicalDevice physical_device)
{
	VkFormat formats[2];
	if (0)
    {
		formats[0] = VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
		glConfig.stencilBits = 8;
	}
    else
    {
		formats[0] = VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
		glConfig.stencilBits = 0;
	}

    int i;
	for (i = 0; i < 2; i++)
    {
		VkFormatProperties props;
		qvkGetPhysicalDeviceFormatProperties(physical_device, formats[i], &props);
		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
			return formats[i];
		}
	}
	ri.Error(ERR_FATAL, "get_depth_format: failed to find depth attachment format");
	return VK_FORMAT_UNDEFINED; // never get here
}


static VkShaderModule create_shader_module(const unsigned char* pBytes, int count)
{
	if (count % 4 != 0) {
		ri.Error(ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not multiple of 4");
	}
	VkShaderModuleCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = count;
	desc.pCode = (const uint32_t*)pBytes;
			   
	VkShaderModule module;
	VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));
	return module;
};


void vk_shutdown()
{
    ri.Printf( PRINT_ALL, "vk_shutdown()\n" );
    unsigned int i = 0, j = 0, k = 0;

	qvkDestroyImage(vk.device, vk.depth_image, NULL);
	qvkFreeMemory(vk.device, vk.depth_image_memory, NULL);
	qvkDestroyImageView(vk.device, vk.depth_image_view, NULL);

	for (i = 0; i < vk.swapchain_image_count; i++)
		qvkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);

	qvkDestroyRenderPass(vk.device, vk.render_pass, NULL);

	qvkDestroyCommandPool(vk.device, vk.command_pool, NULL);

	for (i = 0; i < vk.swapchain_image_count; i++)
		qvkDestroyImageView(vk.device, vk.swapchain_image_views[i], NULL);

	qvkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);
	qvkDestroyDescriptorSetLayout(vk.device, vk.set_layout, NULL);
	qvkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
	qvkDestroyBuffer(vk.device, vk.vertex_buffer, NULL);
	qvkDestroyBuffer(vk.device, vk.index_buffer, NULL);
	qvkFreeMemory(vk.device, vk.geometry_buffer_memory, NULL);

    vk_destroy_sync_primitives();



	qvkDestroyShaderModule(vk.device, vk.single_texture_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.single_texture_clipping_plane_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.single_texture_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_clipping_plane_vs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_mul_fs, NULL);
	qvkDestroyShaderModule(vk.device, vk.multi_texture_add_fs, NULL);

	qvkDestroyPipeline(vk.device, vk.skybox_pipeline, NULL);
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
        {
			qvkDestroyPipeline(vk.device, vk.shadow_volume_pipelines[i][j], NULL);
		}
	
    qvkDestroyPipeline(vk.device, vk.shadow_finish_pipeline, NULL);
	
    
    for (i = 0; i < 2; i++)
		for (j = 0; j < 3; j++)
			for (k = 0; k < 2; k++)
            {
				qvkDestroyPipeline(vk.device, vk.fog_pipelines[i][j][k], NULL);
				qvkDestroyPipeline(vk.device, vk.dlight_pipelines[i][j][k], NULL);
			}

	qvkDestroyPipeline(vk.device, vk.tris_debug_pipeline, NULL);
	qvkDestroyPipeline(vk.device, vk.tris_mirror_debug_pipeline, NULL);
	qvkDestroyPipeline(vk.device, vk.normals_debug_pipeline, NULL);
	qvkDestroyPipeline(vk.device, vk.surface_debug_pipeline_solid, NULL);
	qvkDestroyPipeline(vk.device, vk.surface_debug_pipeline_outline, NULL);
	qvkDestroyPipeline(vk.device, vk.images_debug_pipeline, NULL);

	qvkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
	qvkDestroyDevice(vk.device, NULL);
	qvkDestroySurfaceKHR(vk.instance, vk.surface, NULL);

#ifndef NDEBUG
	qvkDestroyDebugReportCallbackEXT(vk.instance, debug_callback, NULL);
#endif

	qvkDestroyInstance(vk.instance, NULL);

	memset(&vk, 0, sizeof(vk));
	VK_ClearProcAddress();
}



void vk_initialize(void)
{
    uint32_t i = 0;

	qvkGetDeviceQueue(vk.device, vk.queue_family_index, 0, &vk.queue);
#ifndef NDEBUG
	// Create debug callback.
    createDebugCallback();
#endif
	//
	// Swapchain.
	//
	{
		vk.swapchain = create_swapchain(vk.physical_device, vk.device, vk.surface, vk.surface_format);

		VK_CHECK(qvkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, NULL));
		vk.swapchain_image_count = MIN(vk.swapchain_image_count, (uint32_t)MAX_SWAPCHAIN_IMAGES);
		VK_CHECK(qvkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images));

		for (i = 0; i < vk.swapchain_image_count; i++)
        {
			VkImageViewCreateInfo desc;
			desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.image = vk.swapchain_images[i];
			desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
			desc.format = vk.surface_format.format;
			desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			desc.subresourceRange.baseMipLevel = 0;
			desc.subresourceRange.levelCount = 1;
			desc.subresourceRange.baseArrayLayer = 0;
			desc.subresourceRange.layerCount = 1;
			VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &vk.swapchain_image_views[i]));
		}
	}

	//
	// Sync primitives.
	//
    vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		VkCommandPoolCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueFamilyIndex = vk.queue_family_index;

		VK_CHECK(qvkCreateCommandPool(vk.device, &desc, NULL, &vk.command_pool));
	}

	//
	// Command buffer.
	//
	{
		VkCommandBufferAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.commandPool = vk.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &vk.command_buffer));
	}

	//
	// Depth attachment image.
	// 
	{
		VkFormat depth_format = get_depth_format(vk.physical_device);

		// create depth image
		{
			VkImageCreateInfo desc;
			desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.imageType = VK_IMAGE_TYPE_2D;
			desc.format = depth_format;
			desc.extent.width = glConfig.vidWidth;
			desc.extent.height = glConfig.vidHeight;
			desc.extent.depth = 1;
			desc.mipLevels = 1;
			desc.arrayLayers = 1;
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.tiling = VK_IMAGE_TILING_OPTIMAL;
			desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			desc.queueFamilyIndexCount = 0;
			desc.pQueueFamilyIndices = NULL;
			desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &vk.depth_image));
		}

		// allocate depth image memory
		{
			VkMemoryRequirements memory_requirements;
			qvkGetImageMemoryRequirements(vk.device, vk.depth_image, &memory_requirements);

			VkMemoryAllocateInfo alloc_info;
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.pNext = NULL;
			alloc_info.allocationSize = memory_requirements.size;
			alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.depth_image_memory));
			VK_CHECK(qvkBindImageMemory(vk.device, vk.depth_image, vk.depth_image_memory, 0));
		}

		// create depth image view
		{
			VkImageViewCreateInfo desc;
			desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.image = vk.depth_image;
			desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
			desc.format = depth_format;
			desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			desc.subresourceRange.baseMipLevel = 0;
			desc.subresourceRange.levelCount = 1;
			desc.subresourceRange.baseArrayLayer = 0;
			desc.subresourceRange.layerCount = 1;
			VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &vk.depth_image_view));
		}

		VkImageAspectFlags image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
		
        //r_stencilbits->integer
        if (0)
			image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;


	    VkCommandBufferAllocateInfo alloc_info;
	    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	    alloc_info.pNext = NULL;
    	alloc_info.commandPool = vk.command_pool;
    	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    	alloc_info.commandBufferCount = 1;

	    VkCommandBuffer pCB;
	    VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &pCB));

    	VkCommandBufferBeginInfo begin_info;
    	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	    begin_info.pNext = NULL;
    	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	    begin_info.pInheritanceInfo = NULL;

    	VK_CHECK(qvkBeginCommandBuffer(pCB, &begin_info));
	    // recorder(command_buffer);
        {
            VkCommandBuffer command_buffer = pCB;
            record_image_layout_transition(command_buffer, vk.depth_image, 
                image_aspect_flags, 0, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | 
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	
        } 
        VK_CHECK(qvkEndCommandBuffer(pCB));

	    VkSubmitInfo submit_info;
    	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    	submit_info.pNext = NULL;
    	submit_info.waitSemaphoreCount = 0;
    	submit_info.pWaitSemaphores = NULL;
    	submit_info.pWaitDstStageMask = NULL;
    	submit_info.commandBufferCount = 1;
    	submit_info.pCommandBuffers = &pCB;
    	submit_info.signalSemaphoreCount = 0;
    	submit_info.pSignalSemaphores = NULL;

    	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
    	VK_CHECK(qvkQueueWaitIdle(vk.queue));
    	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &pCB);

	}

	//
	// Renderpass.
	//
	{
		VkFormat depth_format = get_depth_format(vk.physical_device);
		vk.render_pass = create_render_pass(vk.device, vk.surface_format.format, depth_format);
	}

	//
	// Framebuffers for each swapchain image.
	//
	{
		VkImageView attachments[2] = {VK_NULL_HANDLE, vk.depth_image_view};

		VkFramebufferCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.renderPass = vk.render_pass;
		desc.attachmentCount = 2;
		desc.pAttachments = attachments;
		desc.width = glConfig.vidWidth;
		desc.height = glConfig.vidHeight;
		desc.layers = 1;

		for (i = 0; i < vk.swapchain_image_count; i++)
		{
			attachments[0] = vk.swapchain_image_views[i]; // set color attachment
			VK_CHECK(qvkCreateFramebuffer(vk.device, &desc, NULL, &vk.framebuffers[i]));
		}
	}

	//
	// Descriptor pool.
	//
	{
		VkDescriptorPoolSize pool_size;
		pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size.descriptorCount = MAX_DRAWIMAGES;

		VkDescriptorPoolCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // used by the cinematic images
		desc.maxSets = MAX_DRAWIMAGES;
		desc.poolSizeCount = 1;
		desc.pPoolSizes = &pool_size;

		VK_CHECK(qvkCreateDescriptorPool(vk.device, &desc, NULL, &vk.descriptor_pool));
	}

	//
	// Descriptor set layout.
	//
	{
		VkDescriptorSetLayoutBinding descriptor_binding;
		descriptor_binding.binding = 0;
		descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_binding.descriptorCount = 1;
		descriptor_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		descriptor_binding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.bindingCount = 1;
		desc.pBindings = &descriptor_binding;

		VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, &vk.set_layout));
	}

	//
	// Pipeline layout.
	//
	{
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 128; // 32 floats

		VkDescriptorSetLayout set_layouts[2] = {vk.set_layout, vk.set_layout};

		VkPipelineLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 2;
		desc.pSetLayouts = set_layouts;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;

		VK_CHECK(qvkCreatePipelineLayout(vk.device, &desc, NULL, &vk.pipeline_layout));
	}

	//
	// Geometry buffers.
	//
	{
		VkBufferCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;

		desc.size = VERTEX_BUFFER_SIZE;
		desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &vk.vertex_buffer));

		desc.size = INDEX_BUFFER_SIZE;
		desc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		VK_CHECK(qvkCreateBuffer(vk.device, &desc, NULL, &vk.index_buffer));

		VkMemoryRequirements vb_memory_requirements;
		qvkGetBufferMemoryRequirements(vk.device, vk.vertex_buffer, &vb_memory_requirements);

		VkMemoryRequirements ib_memory_requirements;
		qvkGetBufferMemoryRequirements(vk.device, vk.index_buffer, &ib_memory_requirements);

		VkDeviceSize mask = ~(ib_memory_requirements.alignment - 1);
		VkDeviceSize index_buffer_offset = (vb_memory_requirements.size + ib_memory_requirements.alignment - 1) & mask;

		uint32_t memory_type_bits = vb_memory_requirements.memoryTypeBits & ib_memory_requirements.memoryTypeBits;
		uint32_t memory_type = find_memory_type(vk.physical_device, memory_type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VkMemoryAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = index_buffer_offset + ib_memory_requirements.size;
		alloc_info.memoryTypeIndex = memory_type;
		VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.geometry_buffer_memory));

		qvkBindBufferMemory(vk.device, vk.vertex_buffer, vk.geometry_buffer_memory, 0);
		qvkBindBufferMemory(vk.device, vk.index_buffer, vk.geometry_buffer_memory, index_buffer_offset);

		void* data;
		VK_CHECK(qvkMapMemory(vk.device, vk.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
		vk.vertex_buffer_ptr = (unsigned char*)data;
		vk.index_buffer_ptr = (unsigned char*)data + index_buffer_offset;
	}

	//
	// Shader modules.
	//
	{

		extern unsigned char single_texture_vert_spv[];
		extern long long single_texture_vert_spv_size;
		vk.single_texture_vs = create_shader_module(single_texture_vert_spv, single_texture_vert_spv_size);

		extern unsigned char single_texture_clipping_plane_vert_spv[];
		extern long long single_texture_clipping_plane_vert_spv_size;
		vk.single_texture_clipping_plane_vs = create_shader_module(single_texture_clipping_plane_vert_spv, single_texture_clipping_plane_vert_spv_size);

		extern unsigned char single_texture_frag_spv[];
		extern long long single_texture_frag_spv_size;
		vk.single_texture_fs = create_shader_module(single_texture_frag_spv, single_texture_frag_spv_size);

		extern unsigned char multi_texture_vert_spv[];
		extern long long multi_texture_vert_spv_size;
		vk.multi_texture_vs = create_shader_module(multi_texture_vert_spv, multi_texture_vert_spv_size);

		extern unsigned char multi_texture_clipping_plane_vert_spv[];
		extern long long multi_texture_clipping_plane_vert_spv_size;
		vk.multi_texture_clipping_plane_vs = create_shader_module(multi_texture_clipping_plane_vert_spv, multi_texture_clipping_plane_vert_spv_size);

		extern unsigned char multi_texture_mul_frag_spv[];
		extern long long multi_texture_mul_frag_spv_size;
		vk.multi_texture_mul_fs = create_shader_module(multi_texture_mul_frag_spv, multi_texture_mul_frag_spv_size);

		extern unsigned char multi_texture_add_frag_spv[];
		extern long long multi_texture_add_frag_spv_size;
		vk.multi_texture_add_fs = create_shader_module(multi_texture_add_frag_spv, multi_texture_add_frag_spv_size);
	}

	//
	// Standard pipelines.
	//
    create_standard_pipelines();

}
