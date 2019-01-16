#include "tr_local.h"

#include "vk_initialize.h"
#include "vk_image.h"
#include "vk_instance.h"
#include "vk_shade_geometry.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "vk_shaders.h"




static void vk_createRenderPass(VkDevice device, VkFormat color_format, VkFormat depth_format, VkRenderPass* pRepass)
{

// Before we can finish creating the pipeline, we need to tell vulkan
// about the framebuffer attachment that will be used while rendering.
// We need to specify how many color and depth buffers there will be,
// how many samples to use for each of them and how their contents 
// should be handled throughout the rendering operations.

	VkAttachmentDescription attachments[2];
	attachments[0].flags = 0;

//  The format of the color attachment should match the format of the
//  swap chain images. 
	attachments[0].format = color_format;
//  have something with the multisampling
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

//  Attachments can also be cleared at the beginning of a render pass
//  instance by setting loadOp/stencilLoadOp of VkAttachmentDescription
//  to VK_ATTACHMENT_LOAD_OP_CLEAR, as described for vkCreateRenderPass.
//  loadOp and stencilLoadOp, specifying how the contents of the attachment
//  are treated before rendering and after rendering.
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

//  specifies that the contents within the render area will be cleared to
//  a uniform value, which is specified when a render pass instance is begun    
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Textueres and framebuffers in Vulkan are represented by VkImage
    // objects with a certain pixel format. however the layout of the
    // pixels in memory can change based on what you're trying to do
    // with an image.
	VkAttachmentReference color_attachment_ref;
	color_attachment_ref.attachment = 0;

    // Images used as color attachment
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
	desc.attachmentCount = 2;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 0;
	desc.pDependencies = NULL;


	VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, pRepass));
}





static VkFormat get_depth_format(VkPhysicalDevice physical_device)
{
	VkFormat formats[2];
	if (1)
    {
		formats[0] = VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
		//glConfig.stencilBits = 8;
	}
    else
    {
		formats[0] = VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
		//glConfig.stencilBits = 0;
	}

    uint32_t i;
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





void vk_shutdown(void)
{
    ri.Printf( PRINT_ALL, "vk_shutdown()\n" );

	qvkDestroyImage(vk.device, vk.depth_image, NULL);
	qvkFreeMemory(vk.device, vk.depth_image_memory, NULL);
	qvkDestroyImageView(vk.device, vk.depth_image_view, NULL);

    // we should delete the framebuffers before the image views
    // and the render pass that they are based on.
    vk_destroyFrameBuffers();

	qvkDestroyRenderPass(vk.device, vk.render_pass, NULL);
	
    //

    vk_destroy_shading_data();

    vk_destroy_sync_primitives();
    
    vk_destroyShaderModules();

//
    vk_destroyGlobalStagePipeline();
//


	vk_clearProcAddress();
}


static void vk_create_command_pool(VkCommandPool* pPool)
{    
    VkCommandPoolCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    desc.queueFamilyIndex = vk.queue_family_index;

    VK_CHECK(qvkCreateCommandPool(vk.device, &desc, NULL, pPool));
}

static void vk_create_command_buffer(VkCommandPool cmd_pool, VkCommandBuffer* pBuf)
{
    VkCommandBufferAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.commandPool = cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, pBuf));
}

void vk_initialize(void)
{
    // This function is responsible for initializing a valid Vulkan subsystem.

    vk_createWindow();
        
    vk_getProcAddress(); 
 

	// Swapchain. vk.physical_device required to be init. 
	vk_createSwapChain(vk.device, vk.surface, vk.surface_format);

	//
	// Sync primitives.
	//
    vk_create_sync_primitives();

	// we have to create a command pool before we can create command buffers
    // command pools manage the memory that is used to store the buffers and
    // command buffers are allocated from them.

    vk_create_command_pool(&vk.command_pool);
	//
	// Command buffer.
	//
    vk_create_command_buffer(vk.command_pool, &vk.command_buffer);

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
        if (1)
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
  
        record_image_layout_transition(pCB, vk.depth_image, 
                image_aspect_flags, 0, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | 
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	

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
	// A render pass represents a collection of attachments, subpasses,
    // and dependencies between the subpasses, and describes how the 
    // attachments are used over the course of the subpasses. The use
    // of a render pass in a command buffer is a render pass instance.
    //
    // An attachment description describes the properties of an attachment
    // including its format, sample count, and how its contents are treated
    // at the beginning and end of each render pass instance.
    //
    // A subpass represents a phase of rendering that reads and writes
    // a subset of the attachments in a render pass. Rendering commands
    // are recorded into a particular subpass of a render pass instance.
    //
    // A subpass description describes the subset of attachments that 
    // is involved in the execution of a subpass. Each subpass can read
    // from some attachments as input attachments, write to some as
    // color attachments or depth/stencil attachments, and perform
    // multisample resolve operations to resolve attachments. A subpass
    // description can also include a set of preserve attachments, 
    // which are attachments that are not read or written by the subpass
    // but whose contents must be preserved throughout the subpass.
    //
    // A subpass uses an attachment if the attachment is a color, 
    // depth/stencil, resolve, or input attachment for that subpass
    // (as determined by the pColorAttachments, pDepthStencilAttachment,
    // pResolveAttachments, and pInputAttachments members of 
    // VkSubpassDescription, respectively). A subpass does not use an
    // attachment if that attachment is preserved by the subpass.
    // The first use of an attachment is in the lowest numbered subpass
    // that uses that attachment. Similarly, the last use of an attachment
    // is in the highest numbered subpass that uses that attachment.
    //
    // The subpasses in a render pass all render to the same dimensions, 
    // and fragments for pixel (x,y,layer) in one subpass can only read
    // attachment contents written by previous subpasses at that same
    // (x,y,layer) location.
    //
    // By describing a complete set of subpasses in advance, render passes
    // provide the implementation an opportunity to optimize the storage 
    // and transfer of attachment data between subpasses. In practice, 
    // this means that subpasses with a simple framebuffer-space dependency
    // may be merged into a single tiled rendering pass, keeping the
    // attachment data on-chip for the duration of a render pass instance. 
    // However, it is also quite common for a render pass to only contain
    // a single subpass.
    //
    // Subpass dependencies describe execution and memory dependencies 
    // between subpasses. A subpass dependency chain is a sequence of
    // subpass dependencies in a render pass, where the source subpass
    // of each subpass dependency (after the first) equals the destination
    // subpass of the previous dependency.
    //
    // Execution of subpasses may overlap or execute out of order with
    // regards to other subpasses, unless otherwise enforced by an 
    // execution dependency. Each subpass only respects submission order
    // for commands recorded in the same subpass, and the vkCmdBeginRenderPass
    // and vkCmdEndRenderPass commands that delimit the render pass - 
    // commands within other subpasses are not included. This affects 
    // most other implicit ordering guarantees.
    //
    // A render pass describes the structure of subpasses and attachments
    // independent of any specific image views for the attachments. 
    // The specific image views that will be used for the attachments,
    // and their dimensions, are specified in VkFramebuffer objects. 
    // Framebuffers are created with respect to a specific render pass
    // that the framebuffer is compatible with (see Render Pass Compatibility).
    // Collectively, a render pass and a framebuffer define the complete
    // render target state for one or more subpasses as well as the 
    // algorithmic dependencies between the subpasses.
    //
    // The various pipeline stages of the drawing commands for a given
    // subpass may execute concurrently and/or out of order, both within 
    // and across drawing commands, whilst still respecting pipeline order.
    // However for a given (x,y,layer,sample) sample location, certain
    // per-sample operations are performed in rasterization order.
    
	vk_createRenderPass(vk.device, vk.surface_format.format, get_depth_format(vk.physical_device), &vk.render_pass);

    vk_createFrameBuffers(vk.render_pass);

	// Pipeline layout.
	// You can use uniform values in shaders, which are globals similar to
    // dynamic state variables that can be changes at the drawing time to
    // alter the behavior of your shaders without having to recreate them.
    // They are commonly used to create texture samplers in the fragment 
    // shader. The uniform values need to be specified during pipeline
    // creation by creating a VkPipelineLayout object.
    
    vk_createPipelineLayout();

	//
	vk_createGeometryBuffers();
	//
	// Shader modules.
	//
	vk_loadShaderModules();

	//
	// Standard pipelines.
	//
    create_standard_pipelines();

}

void vulkanInfo_f( void ) 
{

	// VULKAN

    ri.Printf( PRINT_ALL, "\nActive 3D API: Vulkan\n" );

    // To query general properties of physical devices once enumerated
    VkPhysicalDeviceProperties props;
    qvkGetPhysicalDeviceProperties(vk.physical_device, &props);

    uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
    uint32_t patch = VK_VERSION_PATCH(props.apiVersion);

    const char* device_type;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        device_type = "INTEGRATED_GPU";
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        device_type = "DISCRETE_GPU";
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
        device_type = "VIRTUAL_GPU";
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        device_type = "CPU";
    else
        device_type = "Unknown";

    const char* vendor_name = "unknown";
    if (props.vendorID == 0x1002) {
        vendor_name = "Advanced Micro Devices, Inc.";
    } else if (props.vendorID == 0x10DE) {
        vendor_name = "NVIDIA";
    } else if (props.vendorID == 0x8086) {
        vendor_name = "Intel Corporation";
    }

    ri.Printf(PRINT_ALL, "Vk api version: %d.%d.%d\n", major, minor, patch);
    ri.Printf(PRINT_ALL, "Vk driver version: %d\n", props.driverVersion);
    ri.Printf(PRINT_ALL, "Vk vendor id: 0x%X (%s)\n", props.vendorID, vendor_name);
    ri.Printf(PRINT_ALL, "Vk device id: 0x%X\n", props.deviceID);
    ri.Printf(PRINT_ALL, "Vk device type: %s\n", device_type);
    ri.Printf(PRINT_ALL, "Vk device name: %s\n", props.deviceName);

//    ri.Printf(PRINT_ALL, "\n The maximum number of sampler objects,  
//    as created by vkCreateSampler, which can simultaneously exist on a device is: %d\n", 
//        props.limits.maxSamplerAllocationCount);
//	 4000

    // Look for device extensions
    {
        uint32_t nDevExts = 0;

        // To query the extensions available to a given physical device
        VK_CHECK( qvkEnumerateDeviceExtensionProperties( vk.physical_device, NULL, &nDevExts, NULL) );

        VkExtensionProperties* pDeviceExt = 
            (VkExtensionProperties *) malloc(sizeof(VkExtensionProperties) * nDevExts);

        qvkEnumerateDeviceExtensionProperties(
                vk.physical_device, NULL, &nDevExts, pDeviceExt);



        ri.Printf(PRINT_ALL, "---------- Total Device Extension Supported ---------- \n");
        uint32_t i;
        for (i=0; i<nDevExts; i++)
        {
            ri.Printf(PRINT_ALL, " %s \n", pDeviceExt[i].extensionName);
        }
        ri.Printf(PRINT_ALL, "---------- -------------------------------- ---------- \n");
    }

    ri.Printf(PRINT_ALL, "Vk instance extensions: \n%s\n\n",
            glConfig.extensions_string);


	//
	// Info that for UI display
	//
	strncpy( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
	strncpy( glConfig.renderer_string, props.deviceName, sizeof( glConfig.renderer_string ) );
    if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
         glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;     
	char tmpBuf[128] = {0};

    snprintf(tmpBuf, 128,"Vk api version: %d.%d.%d ", major, minor, patch);
	
    strncpy( glConfig.version_string, tmpBuf, sizeof( glConfig.version_string ) );

}

