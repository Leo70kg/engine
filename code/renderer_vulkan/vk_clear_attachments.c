#include "tr_local.h"
#include "qvk.h"


void vk_clear_attachments(qboolean clear_depth_stencil, qboolean clear_color, float* color)
{
	if (!vk.active)
		return;

	if (!clear_depth_stencil && !clear_color)
		return;

	VkClearAttachment attachments[2];
	uint32_t attachment_count = 0;

	if (clear_depth_stencil) {
		attachments[0].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[0].clearValue.depthStencil.depth = 1.0f;

		if (r_shadows->integer == 2) {
			attachments[0].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			attachments[0].clearValue.depthStencil.stencil = 0;
		}
		attachment_count = 1;
	}

	if (clear_color)
    {
		attachments[attachment_count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		attachments[attachment_count].colorAttachment = 0;
		attachments[attachment_count].clearValue.color.float32[0] = color[0];
  		attachments[attachment_count].clearValue.color.float32[1] = color[1];
		attachments[attachment_count].clearValue.color.float32[2] = color[2];
		attachments[attachment_count].clearValue.color.float32[3] = color[3];
		attachment_count++;
	}

	VkClearRect clear_rect[2];
	clear_rect[0].rect = get_scissor_rect();
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;
	int rect_count = 1;

	// Split viewport rectangle into two non-overlapping rectangles.
	// It's a HACK to prevent Vulkan validation layer's performance warning:
	//		"vkCmdClearAttachments() issued on command buffer object XXX prior to any Draw Cmds.
	//		 It is recommended you use RenderPass LOAD_OP_CLEAR on Attachments prior to any Draw."
	// 
	// NOTE: we don't use LOAD_OP_CLEAR for color attachment when we begin renderpass
	// since at that point we don't know whether we need collor buffer clear (usually we don't).
	if (clear_color) {
		uint32_t h = clear_rect[0].rect.extent.height / 2;
		clear_rect[0].rect.extent.height = h;
		clear_rect[1] = clear_rect[0];
		clear_rect[1].rect.offset.y = h;
		rect_count = 2;
	}

	qvkCmdClearAttachments(vk.command_buffer, attachment_count, attachments, rect_count, clear_rect);
}
