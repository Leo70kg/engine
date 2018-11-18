#include "tr_local.h"


#include "vk_memory.h"



uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	qvkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    uint32_t i;
	for (i = 0; i < memory_properties.memoryTypeCount; i++)
    {
		if ( ((memory_type_bits & (1 << i)) != 0) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
			return i;
		}
	}
	ri.Error(ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties");
	return -1;
}




void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access)
{

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier(cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL);
}
