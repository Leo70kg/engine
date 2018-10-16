#include "qvk.h"
#include "tr_local.h"


static void ensure_staging_buffer_allocation(VkDeviceSize size)
{
	if (vk_world.staging_buffer_size >= size)
		return;

	if (vk_world.staging_buffer != VK_NULL_HANDLE)
		qvkDestroyBuffer(vk.device, vk_world.staging_buffer, NULL);

	if (vk_world.staging_buffer_memory != VK_NULL_HANDLE)
		qvkFreeMemory(vk.device, vk_world.staging_buffer_memory, NULL);

	vk_world.staging_buffer_size = size;

	VkBufferCreateInfo buffer_desc;
	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk_world.staging_buffer));

	VkMemoryRequirements memory_requirements;
	qvkGetBufferMemoryRequirements(vk.device, vk_world.staging_buffer, &memory_requirements);

	uint32_t memory_type = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;
	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk_world.staging_buffer_memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk_world.staging_buffer, vk_world.staging_buffer_memory, 0));

	void* data;
	VK_CHECK(qvkMapMemory(vk.device, vk_world.staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk_world.staging_buffer_ptr = (byte*)data;
}


void vk_upload_image_data(VkImage image, int width, int height, 
        qboolean mipmap, const uint8_t* pixels, int bytes_per_pixel)
{

	VkBufferImageCopy regions[16];
	int num_regions = 0;

	int buffer_size = 0;

	while (qtrue) {
		VkBufferImageCopy region;
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
        region.imageOffset.y = 0;
		region.imageOffset.z = 0;

        region.imageExtent.width = width;
		region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        
		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * bytes_per_pixel;

		if (!mipmap || (width == 1 && height == 1))
			break;

		width >>= 1;
		if (width < 1)
            width = 1;

		height >>= 1;
		if (height < 1)
            height = 1;
	}

	ensure_staging_buffer_allocation(buffer_size);
	memcpy(vk_world.staging_buffer_ptr, pixels, buffer_size);

/*
	record_and_run_commands(vk.command_pool, vk.queue,
    [&image, &num_regions, &regions](VkCommandBuffer command_buffer)
    {

		record_buffer_memory_barrier(command_buffer, vk_world.staging_buffer,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		record_image_layout_transition(command_buffer, image, 
        VK_IMAGE_ASPECT_COLOR_BIT,0, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vkCmdCopyBufferToImage(command_buffer, vk_world.staging_buffer, image, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);

		record_image_layout_transition(command_buffer, image, 
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});
*/

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
	
    //// recorder(command_buffer);
    //// recorder = [&image, &num_regions, &regions](VkCommandBuffer command_buffer)
    {
        VkCommandBuffer cb = command_buffer;
            record_buffer_memory_barrier(cb, vk_world.staging_buffer,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

	    record_image_layout_transition(cb, image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	    qvkCmdCopyBufferToImage(cb, vk_world.staging_buffer, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);

	    record_image_layout_transition(cb, image,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    ////////
    
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

	VkResult res = qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE);	
	if (res < 0) 
		ri.Error(ERR_FATAL,
        "vk_upload_image_data: error code %d returned by qvkQueueSubmit", res);


    VK_CHECK(qvkQueueWaitIdle(vk.queue));
	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);

    ri.Printf( PRINT_ALL, "vk_upload_image_data ok.\n");
}
