#include "qvk.h"
#include "tr_local.h"
#include "vk_clear_attachments.h"


void vk_begin_frame(void)
{
    
	VK_CHECK(qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
        vk.image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));

	VK_CHECK(qvkWaitForFences(vk.device, 1, &vk.rendering_finished_fence,
                VK_FALSE, 1e9));
	VK_CHECK(qvkResetFences(vk.device, 1, &vk.rendering_finished_fence));

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
	submit_info.pWaitSemaphores = &vk.image_acquired;
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &vk.rendering_finished;


    VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, vk.rendering_finished_fence));

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.rendering_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.swapchain_image_index;
	present_info.pResults = NULL;
	
    VkResult result = qvkQueuePresentKHR(vk.queue, &present_info);
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if( r_fullscreen->integer == 1)
		{
			r_fullscreen->integer = 0;
            r_mode->integer = 3;
		}

        ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");
        // hasty prevent crash.
    }
    else
    {
        VK_CHECK(result)
    }
}

/*
    VK_ERROR_OUT_OF_DATE_KHR

A surface has changed in such a way that it is no longer compatible with
the swapchain, and further presentation requests using the swapchain will
fail. Applications must query the new surface properties and recreate 
their swapchain if they wish to continue presenting to the surface.

*/
