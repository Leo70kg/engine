#include "qvk.h"
#include "tr_local.h"
#include "vk_clear_attachments.h"
#include "vk_memory.h"
#include "vk_frame.h"
#include "Vk_Instance.h"

/*
    Fences are a synchronization primitive that can be used to insert a dependency
    from a queue to the host. Fences have two states - signaled and unsignaled.
    A fence can be signaled as part of the execution of a queue submission command. 
    Fences can be unsignaled on the host with vkResetFences. Fences can be waited
    on by the host with the vkWaitForFences command, and the current state can be
    queried with vkGetFenceStatus.
*/


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

   
*/

void vk_create_sync_primitives(void)
{
    VkSemaphoreCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;

    VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &image_acquired));
    VK_CHECK(qvkCreateSemaphore(vk.device, &desc, NULL, &rendering_finished));

/*

VkResult vkCreateFence( VkDevice device, const VkFenceCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkFence* pFence);
    
    device is the logical device that creates the fence.
  
    pCreateInfo is a pointer to an instance of the VkFenceCreateInfo structure
    which contains information about how the fence is to be created.

    pAllocator controls host memory allocation as described in the
    Memory Allocation chapter.

    pFence points to a handle in which the resulting fence object is returned.

    VK_FENCE_CREATE_SIGNALED_BIT specifies that the fence object is created
    in the signaled state. Otherwise, it is created in the unsignaled state.
*/

    VkFenceCreateInfo fence_desc;
    fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_desc.pNext = NULL;
    fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(qvkCreateFence(vk.device, &fence_desc, NULL, &rendering_finished_fence));
}


void vk_destroy_sync_primitives(void)
{
	qvkDestroySemaphore(vk.device, image_acquired, NULL);
	qvkDestroySemaphore(vk.device, rendering_finished, NULL);
	qvkDestroyFence(vk.device, rendering_finished_fence, NULL);
}


void vk_begin_frame(void)
{
/*  
   An application can acquire use of a presentable image with 
   vkAcquireNextImageKHR. After acquiring a presentable image 
   and before modifying it, the application must use a 
   synchronization primitive to ensure that the presentation 
   engine has finished reading from the image. The application can
   then transition the image¡¯s layout, queue rendering commands to
   it, etc. Finally, the application presents the image with 
   vkQueuePresentKHR, which releases the acquisition of the image.
*/

	VK_CHECK(qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
        image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));
/*
    user could call method vkWaitForFences to wait for completion. 
    A fence is a very heavyweight synchronization primitive as it requires 
    the GPU to flush all caches at least, and potentially some additional 
    synchronization. Due to those costs, fences should be used sparingly. 
    In particular, try to group per-frame resources and track them together
*/
	VK_CHECK(qvkWaitForFences(vk.device, 1, &rendering_finished_fence,
                VK_FALSE, 1e9));
    
	VK_CHECK(qvkResetFences(vk.device, 1, &rendering_finished_fence));

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK(qvkBeginCommandBuffer(vk.command_buffer, &begin_info));

	// Ensure visibility of geometry buffers writes.
	record_buffer_memory_barrier(vk.command_buffer, vk.vertex_buffer,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

	record_buffer_memory_barrier(vk.command_buffer, vk.index_buffer,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT);

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

    set_depth_attachment(VK_FALSE);
	
    vk.xyz_elements = 0;
	vk.color_st_elements = 0;
	vk.index_buffer_offset = 0;
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


    VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, rendering_finished_fence));


//	VK_CHECK(qvkQueuePresentKHR(vk.queue, &present_info));   

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


    // After queueing all rendering commands and transitioning the image
    // to the correct layout, to queue an image for presentation

    // queue is a queue that is capable of presentation to the target 
    // surface¡¯s platform on the same device as the image¡¯s swapchain.
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
