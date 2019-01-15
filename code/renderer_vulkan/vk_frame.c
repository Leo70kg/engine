#include "tr_local.h"
#include "tr_cvar.h"
#include "vk_instance.h"
#include "vk_shade_geometry.h"
#include "vk_frame.h"
//  Synchronization of access to resources is primarily the responsibility
//  of the application in Vulkan. The order of execution of commands with
//  respect to the host and other commands on the device has few implicit
//  guarantees, and needs to be explicitly specified. Memory caches and 
//  other optimizations are also explicitly managed, requiring that the
//  flow of data through the system is largely under application control.
//  Whilst some implicit guarantees exist between commands, five explicit
//  synchronization mechanisms are exposed by Vulkan:
//
//
//                          Fences
//
//  Fences can be used to communicate to the host that execution of some 
//  task on the device has completed. 
//
//  Fences are a synchronization primitive that can be used to insert a dependency
//  from a queue to the host. Fences have two states - signaled and unsignaled.
//  A fence can be signaled as part of the execution of a queue submission command. 
//  Fences can be unsignaled on the host with vkResetFences. Fences can be waited
//  on by the host with the vkWaitForFences command, and the current state can be
//  queried with vkGetFenceStatus.
//
//
//                          Semaphores
//
//  Semaphores can be used to control resource access across multiple queues.
//
//                          Events
//
//  Events provide a fine-grained synchronization primitive which can be
//  signaled either within a command buffer or by the host, and can be 
//  waited upon within a command buffer or queried on the host.
//
//                          Pipeline Barriers
//
//  Pipeline barriers also provide synchronization control within a command buffer,
//  but at a single point, rather than with separate signal and wait operations.
//
//                          Render Passes
//
//  Render passes provide a useful synchronization framework for most rendering tasks,
//  built upon the concepts in this chapter. Many cases that would otherwise need an
//  application to use other synchronization primitives can be expressed more 
//  efficiently as part of a render pass.
//
//



VkSemaphore image_acquired;
VkSemaphore rendering_finished;
VkFence rendering_finished_fence;

/*
   Use of a presentable image must occur only after the image is
   returned by vkAcquireNextImageKHR, and before it is presented by
   vkQueuePresentKHR. This includes transitioning the image layout
   and rendering commands.


   The presentation engine is an abstraction for the platform¡¯s compositor
   or display engine. The presentation engine controls the order in which
   presentable images are acquired for use by the application.

   This allows the platform to handle situations which require out-of-order
   return of images after presentation. At the same time, it allows the 
   application to generate command buffers referencing all of the images in
   the swapchain at initialization time, rather than in its main loop.

   Host access to fence must be externally synchronized.

   When a fence is submitted to a queue as part of a queue submission command, 
   it defines a memory dependency on the batches that were submitted as part
   of that command, and defines a fence signal operation which sets the fence
   to the signaled state.
*/

void vk_create_sync_primitives(void)
{
    VkSemaphoreCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;

    // vk.device is the logical device that creates the semaphore.
    // &desc is a pointer to an instance of the VkSemaphoreCreateInfo structure
    // which contains information about how the semaphore is to be created.
    // When created, the semaphore is in the unsignaled state.
    VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &image_acquired));
    VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &rendering_finished));


    VkFenceCreateInfo fence_desc;
    fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_desc.pNext = NULL;
    
    // VK_FENCE_CREATE_SIGNALED_BIT specifies that the fence object
    // is created in the signaled state. Otherwise, it is created 
    // in the unsignaled state.
    fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // vk.device is the logical device that creates the fence.
    // fence_desc is an instance of the VkFenceCreateInfo structure
    // pAllocator controls host memory allocation as described
    // in the Memory Allocation chapter. which contains information
    // about how the fence is to be created.
    // "rendering_finished_fence" is a handle in which the resulting
    // fence object is returned.

    VK_CHECK(qvkCreateFence(vk.device, &fence_desc, NULL, &rendering_finished_fence));
}



void vk_destroy_sync_primitives(void)
{
    ri.Printf(PRINT_ALL, " Destroy image_acquired rendering_finished rendering_finished_fence\n");

    qvkDestroySemaphore(vk.device, image_acquired, NULL);
	qvkDestroySemaphore(vk.device, rendering_finished, NULL);

    // To destroy a fence, 
	qvkDestroyFence(vk.device, rendering_finished_fence, NULL);
}


void vk_createFrameBuffers(void)
{
    // Framebuffers for each swapchain image.
	// The attachments specified during render pass creation are bound
    // by wrapping them into a VkFramebuffer object. A framebuffer object
    // references all of the VkImageView objects that represent the attachments
    // The image that we have to use as attachment depends on which image
    // the the swap chain returns when we retrieve one for presentation
    // this means that we have to create a framebuffer for all of the images
    // in the swap chain and use the one that corresponds to the retrieved
    // image at draw time.

    ri.Printf(PRINT_ALL, " Create vk.framebuffers \n");


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
    uint32_t i;
    for (i = 0; i < vk.swapchain_image_count; i++)
    {
        attachments[0] = vk.swapchain_image_views[i]; // set color attachment
        VK_CHECK(qvkCreateFramebuffer(vk.device, &desc, NULL, &vk.framebuffers[i]));
    }
}

void vk_destroyFrameBuffers(void)
{
    ri.Printf(PRINT_ALL, " Destroy vk.framebuffers vk.swapchain_image_views vk.swapchain\n");

    uint32_t i;
	for (i = 0; i < vk.swapchain_image_count; i++)
		qvkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);

    for (i = 0; i < vk.swapchain_image_count; i++)
		qvkDestroyImageView(vk.device, vk.swapchain_image_views[i], NULL);

    qvkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
}


void vk_begin_frame(void)
{
  
    //  An application can acquire use of a presentable image with 
    //  vkAcquireNextImageKHR. After acquiring a presentable image 
    //  and before modifying it, the application must use a 
    //  synchronization primitive to ensure that the presentation 
    //  engine has finished reading from the image. 
    //  The application can then transition the image's layout, 
    //  queue rendering commands to it, etc. Finally, 
    //  the application presents the image with vkQueuePresentKHR, 
    //  which releases the acquisition of the image.


	VK_CHECK(qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
        image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));


    //  User could call method vkWaitForFences to wait for completion. 
    //  A fence is a very heavyweight synchronization primitive as it
    //  requires the GPU to flush all caches at least, and potentially
    //  some additional synchronization. Due to those costs, fences
    //  should be used sparingly. In particular, try to group per-frame
    //  resources and track them together. To wait for one or more 
    //  fences to enter the signaled state on the host,
    //  call qvkWaitForFences.

    //  If the condition is satisfied when vkWaitForFences is called, 
    //  then vkWaitForFences returns immediately. If the condition is
    //  not satisfied at the time vkWaitForFences is called, then
    //  vkWaitForFences will block and wait up to timeout nanoseconds
    //  for the condition to become satisfied.

	VK_CHECK(qvkWaitForFences(vk.device, 1, &rendering_finished_fence, VK_FALSE, 1e9));
   
    //  To set the state of fences to unsignaled from the host
    //  "1" is the number of fences to reset. 
    //  "rendering_finished_fence" is the fence handle to reset.
	VK_CHECK(qvkResetFences(vk.device, 1, &rendering_finished_fence));

    //  commandBuffer must not be in the recording or pending state.
    //  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT specifies that
    //  each recording of the command buffer will only be submitted once,
    //  and the command buffer will be reset and recorded again between each submission.
    
	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

    // To begin recording a command buffer
    // begin_info is an instance of the VkCommandBufferBeginInfo structure,
    // which defines additional information about how the command buffer begins recording.
	VK_CHECK(qvkBeginCommandBuffer(vk.command_buffer, &begin_info));

	// Ensure visibility of geometry buffers writes.


{

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = vk_getIndexBuffer();
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

    // vkCmdPipelineBarrier is a synchronization command that inserts a dependency
    // between commands submitted to the same queue, or between commands in the 
    // same subpass. When vkCmdPipelineBarrier is submitted to a queue, it defines
    // a memory dependency between commands that were submitted before it, and 
    // those submitted after it.
    //
    // If vkCmdPipelineBarrier was recorded outside a render pass instance, 
    // the first synchronization scope includes all commands that occur earlier
    // in submission order. 
    //
    // If vkCmdPipelineBarrier was recorded inside a render pass instance, 
    // the first synchronization scope includes only commands that occur
    // earlier in submission order within the same subpass. In either case,
    // the first synchronization scope is limited to operations on the pipeline
    // stages determined by the source stage mask specified by srcStageMask.
    //
    // If vkCmdPipelineBarrier was recorded outside a render pass instance, 
    // the second synchronization scope includes all commands that occur later
    // in submission order. If vkCmdPipelineBarrier was recorded inside a 
    // render pass instance, the second synchronization scope includes only
    // commands that occur later in submission order within the same subpass.
    // In either case, the second synchronization scope is limited to operations
    // on the pipeline stages determined by the destination stage mask specified
    // by dstStageMask.
    //
    // The first access scope is limited to access in the pipeline stages
    // determined by the source stage mask specified by srcStageMask. 
    // Within that, the first access scope only includes the first access
    // scopes defined by elements of the pMemoryBarriers, pBufferMemoryBarriers
    // and pImageMemoryBarriers arrays, which each define a set of memory barriers.
    // If no memory barriers are specified, then the first access scope includes
    // no accesses.
    //
    // The second access scope is limited to access in the pipeline stages
    // determined by the destination stage mask specified by dstStageMask.
    // Within that, the second access scope only includes the second
    // access scopes defined by elements of the pMemoryBarriers,
    // pBufferMemoryBarriers and pImageMemoryBarriers arrays, 
    // which each define a set of memory barriers. If no memory barriers
    // are specified, then the second access scope includes no accesses.
    //
    // If dependencyFlags includes VK_DEPENDENCY_BY_REGION_BIT,
    // then any dependency framebuffer-space pipeline stages is 
    // framebuffer-local - otherwise it is framebuffer-global.

    //To record a pipeline barrier
    
    // VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT specifies read access to a vertex buffer
    // as part of a drawing command, bound by vkCmdBindVertexBuffers.
	barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	qvkCmdPipelineBarrier(vk.command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    // VK_ACCESS_INDEX_READ_BIT specifies read access to an index buffer 
    // as part of an indexed drawing command, bound by vkCmdBindIndexBuffer.
    barrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
    qvkCmdPipelineBarrier(vk.command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

}


	// Begin render pass.
	VkClearValue clear_values[2];
	/// ignore clear_values[0] which corresponds to color attachment
	clear_values[1].depthStencil.depth = 1.0;
	clear_values[1].depthStencil.stencil = 0;

	VkRenderPassBeginInfo render_pass_begin_info;
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = vk.render_pass;
	render_pass_begin_info.framebuffer = vk.framebuffers[vk.swapchain_image_index];
	render_pass_begin_info.renderArea.offset.x = 0;
    render_pass_begin_info.renderArea.offset.y = 0;

	render_pass_begin_info.renderArea.extent.width = glConfig.vidWidth;
    render_pass_begin_info.renderArea.extent.height = glConfig.vidHeight;

    render_pass_begin_info.clearValueCount = 2;
	render_pass_begin_info.pClearValues = clear_values;

	qvkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

}


void vk_end_frame(void)
{
	qvkCmdEndRenderPass(vk.command_buffer);
	
    VK_CHECK(qvkEndCommandBuffer(vk.command_buffer));


	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &image_acquired;
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &rendering_finished;


    //  queue is the queue that the command buffers will be submitted to.
    //  1 is the number of elements in the pSubmits array.
    //  pSubmits is a pointer to an array of VkSubmitInfo structures,
    //  each specifying a command buffer submission batch.
    //
    //  rendering_finished_fence is an optional handle to a fence to be signaled 
    //  once all submitted command buffers have completed execution. 
    //  If fence is not VK_NULL_HANDLE, it defines a fence signal operation.
    //
    //  Submission can be a high overhead operation, and applications should 
    //  attempt to batch work together into as few calls to vkQueueSubmit as possible.
    //
    //  vkQueueSubmit is a queue submission command, with each batch defined
    //  by an element of pSubmits as an instance of the VkSubmitInfo structure.
    //  Batches begin execution in the order they appear in pSubmits, but may
    //  complete out of order.
    //
    //  Fence and semaphore operations submitted with vkQueueSubmit 
    //  have additional ordering constraints compared to other 
    //  submission commands, with dependencies involving previous and
    //  subsequent queue operations. 
    //
    //  The order that batches appear in pSubmits is used to determine
    //  submission order, and thus all the implicit ordering guarantees
    //  that respect it. Other than these implicit ordering guarantees
    //  and any explicit synchronization primitives, these batches may
    //  overlap or otherwise execute out of order. If any command buffer
    //  submitted to this queue is in the executable state, it is moved
    //  to the pending state. Once execution of all submissions of a 
    //  command buffer complete, it moves from the pending state,
    //  back to the executable state.
    //
    //  If a command buffer was recorded with the 
    //  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT flag,
    //  it instead moves back to the invalid state.
       
    //  To submit command buffers to a queue 
    VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, rendering_finished_fence));


    // After queueing all rendering commands and transitioning the
    // image to the correct layout, to queue an image for presentation


    VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &rendering_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.swapchain_image_index;
	present_info.pResults = NULL;

    // Each element of pSwapchains member of pPresentInfo 
    // must be a swapchain that is created for a surface 
    // for which presentation is supported from queue as
    // determined using a call to vkGetPhysicalDeviceSurfaceSupportKHR

    // queue is a queue that is capable of presentation to the target 
    // surface's platform on the same device as the image's swapchain.
    // pPresentInfo is a pointer to an instance of the VkPresentInfoKHR
    // structure specifying the parameters of the presentation.

    VkResult result = qvkQueuePresentKHR(vk.queue, &present_info);
    if(result == VK_SUCCESS)
    {
        return;
    }
    else if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if( r_fullscreen->integer == 1)
		{
			r_fullscreen->integer = 0;
            r_mode->integer = 3;
		}

        ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");
        // hasty prevent crash.
    }
}

/*

A surface has changed in such a way that it is no longer compatible with
the swapchain, and further presentation requests using the swapchain will
fail. Applications must query the new surface properties and recreate 
their swapchain if they wish to continue presenting to the surface.

VK_IMAGE_LAYOUT_PRESENT_SRC_KHR must only be used for presenting a 
presentable image for display. A swapchain¡¯s image must be transitioned
to this layout before calling vkQueuePresentKHR, and must be transitioned
away from this layout after calling vkAcquireNextImageKHR.

*/


// vulkan does not have the concept of a "default framebuffer",
// hence it requires an infrastruture that will own the buffers
// we will render to before we visualize them on the screen.
// This infrastructure is known as the swap chain and must be
// created explicity in vulkan. The swap chain is essentially
// a queue of images that are waiting to be presented to the
// screen.
//
// The general purpose of the swap chain is to synchronize the
// presentation of images with the refresh rate of the screen.

// 1) Basic surface capabilities (min/max number of images in the
//    swap chain, min/max number of images in the swap chain).

// 2) Surcface formats(pixel format, color space)

// 3) Available presentation modes

void vk_createSwapChain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format)
{
    //To query the basic capabilities of a surface, needed in order to create a swapchain

	VkSurfaceCapabilitiesKHR surface_caps;
	VK_CHECK(qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));

    // The swap extent id the resolution of the swap chain images
    // and its almost always exactly equal yo the resolution of 
    // the window that we're drawing to.
	VkExtent2D image_extent = surface_caps.currentExtent;
	if ( (image_extent.width == 0xffffffff) && (image_extent.height == 0xffffffff))
    {
		image_extent.width = MIN(surface_caps.maxImageExtent.width, MAX(surface_caps.minImageExtent.width, 640u));
		image_extent.height = MIN(surface_caps.maxImageExtent.height, MAX(surface_caps.minImageExtent.height, 480u));
	}

    //ri.Printf(PRINT_ALL, " image_extent.width: %d, image_extent.height: %d",
    //            image_extent.width, image_extent.height);

	// VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
	if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
		ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

	// VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
	if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
		ri.Error(ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");


    // The presentation is arguably the most impottant setting for the swap chain
    // because it represents the actual conditions for showing images to the screen
    // There four possible modes available in Vulkan:
    
    // 1) VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application
    //    are transferred to the screen right away, which may result in tearing.
    //
    // 2) VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display
    //    takes an image from the front of the queue when the display is refreshed
    //    and the program inserts rendered images at the back of the queue. If the
    //    queue is full then the program has to wait. This is most similar to 
    //    vertical sync as found in modern games
    //
    // 3) VK_PRESENT_MODE_FIFO_RELAXED_KHR: variation of 2)
    //
    // 4) VK_PRESENT_MODE_MAILBOX_KHR: another variation of 2), the image already
    //    queued are simply replaced with the newer ones. This mode can be used
    //    to avoid tearing significantly less latency issues than standard vertical
    //    sync that uses double buffering.
    //
    // we have to look for the best mode available.
	// determine present mode and swapchain image count
    VkPresentModeKHR present_mode;

    // The number of images in the swap chain, essentially the queue length
    // The implementation specifies the minimum amount of images to functions
    // properly
    uint32_t image_count;

    {
        ri.Printf(PRINT_ALL, "\n-------- Determine present mode --------\n");
        
        uint32_t nPM, i;
        qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &nPM, NULL);

        VkPresentModeKHR *pPresentModes = 
            (VkPresentModeKHR *) malloc( nPM * sizeof(VkPresentModeKHR) );

        qvkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device, surface, &nPM, pPresentModes);


        ri.Printf(PRINT_ALL, "Minimaal mumber ImageCount required: %d, Total %d present mode supported: \n",surface_caps.minImageCount, nPM);

        VkBool32 mailbox_supported = 0;
        VkBool32 immediate_supported = 0;

        for ( i = 0; i < nPM; i++)
        {
            switch(pPresentModes[i])
            {
                case VK_PRESENT_MODE_IMMEDIATE_KHR:
                    ri.Printf(PRINT_ALL, " VK_PRESENT_MODE_IMMEDIATE_KHR \n");
                    immediate_supported = VK_TRUE;
                    break;
                case VK_PRESENT_MODE_MAILBOX_KHR:
                    ri.Printf(PRINT_ALL, " VK_PRESENT_MODE_MAILBOX_KHR \n");
                    mailbox_supported = VK_TRUE;
                    break;
                case VK_PRESENT_MODE_FIFO_KHR:
                    ri.Printf(PRINT_ALL, " VK_PRESENT_MODE_FIFO_KHR \n");
                    break;
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                    ri.Printf(PRINT_ALL, " VK_PRESENT_MODE_FIFO_RELAXED_KHR \n");
                    break;
                default:
                    ri.Printf(PRINT_ALL, " This device do not support presentation %d\n", pPresentModes[i]);
                    break;
            }
         }

        free(pPresentModes);


        if (mailbox_supported)
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            image_count = MAX(3u, surface_caps.minImageCount);
            
            ri.Printf(PRINT_ALL, "\n We choose VK_PRESENT_MODE_MAILBOX_KHR mode, minImageCount: %d. \n", image_count);
        }
        else if(immediate_supported)
        {
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            image_count = MAX(2u, surface_caps.minImageCount);

            ri.Printf(PRINT_ALL, "\n We choose VK_PRESENT_MODE_IMMEDIATE_KHR mode, minImageCount: %d. \n", image_count);
        }
        else
        {
            // VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available.
            present_mode = VK_PRESENT_MODE_FIFO_KHR;
            image_count = MAX(2u, surface_caps.minImageCount);
        }

        if (surface_caps.maxImageCount > 0) {
            image_count = MIN(image_count, surface_caps.maxImageCount) + 1;
        }

        ri.Printf(PRINT_ALL, "\n-------- ----------------------- --------\n");
    }


	// create swap chain
    {
        ri.Printf(PRINT_ALL, "\n-------- Create vk.swapchain --------\n");

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
        
        // render images to a separate image first to perform operations like post-processing
        desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        
        // An image is owned by one queue family at a time and ownership
        // must be explicitly transfered before using it in an another
        // queue family. This option offers the best performance.
        desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        desc.queueFamilyIndexCount = 0;
        desc.pQueueFamilyIndices = NULL;
        
        // we can specify that a certain transform should be applied to
        // images in the swap chain if it is support, like a 90 degree
        // clockwise rotation  or horizontal flip, To specify that you
        // do not want any transformation, simply dprcify the current
        // transformation
        desc.preTransform = surface_caps.currentTransform;

        // The compositeAlpha field specifies if the alpha channel
        // should be used for blending with other windows int the
        // windows system. 
        desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        desc.presentMode = present_mode;
        
        // we don't care about the color of pixels that are obscured.
        desc.clipped = VK_TRUE;

        // With Vulkan it's possible that your swap chain becomes invalid or unoptimized
        // while your application is running, for example because the window was resized.
        // In that case the swap chain actually needs to be recreated from scratch and a
        // reference to the old one must be specified in this field.
        desc.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(qvkCreateSwapchainKHR(device, &desc, NULL, &vk.swapchain));
    }

    //
    {
        VK_CHECK(qvkGetSwapchainImagesKHR(device, vk.swapchain, &vk.swapchain_image_count, NULL));
        vk.swapchain_image_count = MIN(vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES);
        VK_CHECK(qvkGetSwapchainImagesKHR(device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images_array));

        uint32_t i;
        for (i = 0; i < vk.swapchain_image_count; i++)
        {
            VkImageViewCreateInfo desc;
            desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            desc.pNext = NULL;
            desc.flags = 0;
            desc.image = vk.swapchain_images_array[i];
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
            VK_CHECK(qvkCreateImageView(device, &desc, NULL, &vk.swapchain_image_views[i]));
        }
    }
}
