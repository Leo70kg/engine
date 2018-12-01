#include "tr_local.h"
#include "qvk.h"

#include "vk_read_pixels.h"
#include "Vk_Instance.h"
#include "vk_image.h"

extern glconfig_t	glConfig;

void record_image_layout_transition( 
        VkCommandBuffer command_buffer,
        VkImage image,
        VkImageAspectFlags image_aspect_flags,
        VkAccessFlags src_access_flags,
        VkImageLayout old_layout,
        VkAccessFlags dst_access_flags,
        VkImageLayout new_layout )
{

	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access_flags;
	barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,	0, NULL, 0, NULL, 1, &barrier);
}



void vk_read_pixels(unsigned char* buffer)
{

    ri.Printf(PRINT_ALL, " vk_read_pixels() \n\n");

	qvkDeviceWaitIdle(vk.device);

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	VkImageCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	desc.extent.width = glConfig.vidWidth;
	desc.extent.height = glConfig.vidHeight;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image;
	VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &image));

	VkMemoryRequirements memory_requirements;
	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);
	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VkDeviceMemory memory;
	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, image, memory, 0));

  
    {

        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = NULL;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = NULL;

        VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));

        //recorder(command_buffer);
        record_image_layout_transition(command_buffer, 
                vk.swapchain_images[vk.swapchain_image_index], 
                VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_MEMORY_READ_BIT, 
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_READ_BIT, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        record_image_layout_transition(command_buffer, 
                image, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);


        VK_CHECK(qvkEndCommandBuffer(command_buffer));

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;

        VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(qvkQueueWaitIdle(vk.queue));
        qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);
    }


    //////////////////////////////////////////////////////////////////////
	
    // Check if we can use vkCmdBlitImage for the given 
    // source and destination image formats.
	VkBool32 blit_enabled = 1;
	{
		VkFormatProperties formatProps;
		qvkGetPhysicalDeviceFormatProperties(
                vk.physical_device, vk.surface_format.format, &formatProps );

		if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0)
			blit_enabled = 0;

		qvkGetPhysicalDeviceFormatProperties(
                vk.physical_device, VK_FORMAT_R8G8B8A8_UNORM, &formatProps );

		if ((formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0)
			blit_enabled = 0;
	}

	if (blit_enabled)
    {
        ri.Printf(PRINT_ALL, " blit_enabled \n\n");

        VkCommandBufferAllocateInfo alloc_info;
    	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    	alloc_info.pNext = NULL;
    	alloc_info.commandPool = vk.command_pool;
    	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    	alloc_info.commandBufferCount = 1;

    	VkCommandBuffer command_buffer;
    	VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

    	VkCommandBufferBeginInfo begin_info;
    	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    	begin_info.pNext = NULL;
    	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    	begin_info.pInheritanceInfo = NULL;

    	VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));
        //	recorder(command_buffer);
        {
            VkImageBlit region;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;

            region.srcOffsets[0].x = 0;
            region.srcOffsets[0].y = 0;
            region.srcOffsets[0].z = 0;

            region.srcOffsets[1].x = glConfig.vidWidth;
            region.srcOffsets[1].y = glConfig.vidHeight;
            region.srcOffsets[1].z = 1;

            region.dstSubresource = region.srcSubresource;
            region.dstOffsets[0] = region.srcOffsets[0];
            region.dstOffsets[1] = region.srcOffsets[1];

            qvkCmdBlitImage(command_buffer,
                    vk.swapchain_images[vk.swapchain_image_index], 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                    VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
        }

	    VK_CHECK(qvkEndCommandBuffer(command_buffer));

    	VkSubmitInfo submit_info;
    	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    	submit_info.pNext = NULL;
    	submit_info.waitSemaphoreCount = 0;
    	submit_info.pWaitSemaphores = NULL;
    	submit_info.pWaitDstStageMask = NULL;
    	submit_info.commandBufferCount = 1;
    	submit_info.pCommandBuffers = &command_buffer;
    	submit_info.signalSemaphoreCount = 0;
    	submit_info.pSignalSemaphores = NULL;

    	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
    	VK_CHECK(qvkQueueWaitIdle(vk.queue));
    	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);   
	}
    else
    {

        VkQueue queue = vk.queue;

	    VkCommandBufferAllocateInfo alloc_info;
    	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    	alloc_info.pNext = NULL;
    	alloc_info.commandPool = vk.command_pool;
    	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    	alloc_info.commandBufferCount = 1;

	    VkCommandBuffer command_buffer;
    	VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

	    VkCommandBufferBeginInfo begin_info;
    	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    	begin_info.pNext = NULL;
    	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    	begin_info.pInheritanceInfo = NULL;

	    VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));
	    {
            ////   recorder(command_buffer);
            VkImageCopy region;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;
            region.srcOffset.x = 0;
            region.srcOffset.y = 0;
            region.srcOffset.z = 0;
            region.dstSubresource = region.srcSubresource;
            region.dstOffset = region.srcOffset;
            region.extent.width = glConfig.vidWidth;
            region.extent.height = glConfig.vidHeight;
            region.extent.depth = 1;

            qvkCmdCopyImage(command_buffer, 
                    vk.swapchain_images[vk.swapchain_image_index], 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		
        }
        VK_CHECK(qvkEndCommandBuffer(command_buffer));

	    VkSubmitInfo submit_info;
    	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    	submit_info.pNext = NULL;
    	submit_info.waitSemaphoreCount = 0;
    	submit_info.pWaitSemaphores = NULL;
    	submit_info.pWaitDstStageMask = NULL;
    	submit_info.commandBufferCount = 1;
    	submit_info.pCommandBuffers = &command_buffer;
    	submit_info.signalSemaphoreCount = 0;
    	submit_info.pSignalSemaphores = NULL;

    	VK_CHECK(qvkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(qvkQueueWaitIdle(queue));
    	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);
	}



	// Copy data from destination image to memory buffer.
	VkImageSubresource subresource;
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;
	VkSubresourceLayout layout;
	qvkGetImageSubresourceLayout(vk.device, image, &subresource, &layout);


    // To retrieve a host virtual address pointer to
    // a region of a mappable memory objec
	unsigned char* data;
	VK_CHECK(qvkMapMemory(vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data));
	data += layout.offset;

	unsigned char* buffer_ptr = buffer + 
    glConfig.vidWidth * (glConfig.vidHeight - 1) * 4;
	
    int j = 0;
    for (j = 0; j < glConfig.vidHeight; j++)
    {
		memcpy(buffer_ptr, data, glConfig.vidWidth * 4);
		buffer_ptr -= glConfig.vidWidth * 4;
		data += layout.rowPitch;
	}


	if (!blit_enabled)
    {
        ri.Printf(PRINT_ALL, "blit_enabled = 0 \n\n");

		//auto fmt = vk.surface_format.format;
		VkBool32 swizzle_components = 
            (vk.surface_format.format == VK_FORMAT_B8G8R8A8_SRGB ||
             vk.surface_format.format == VK_FORMAT_B8G8R8A8_UNORM ||
             vk.surface_format.format == VK_FORMAT_B8G8R8A8_SNORM);
        
		if (swizzle_components)
        {
			buffer_ptr = buffer;
            unsigned int i = 0;
            unsigned int pixels = glConfig.vidWidth * glConfig.vidHeight;
			for (i = 0; i < pixels; i++)
            {
				unsigned char tmp = buffer_ptr[0];
				buffer_ptr[0] = buffer_ptr[2];
				buffer_ptr[2] = tmp;
				buffer_ptr += 4;
			}
		}
	}

	qvkDestroyImage(vk.device, image, NULL);
	qvkFreeMemory(vk.device, memory, NULL);
}

