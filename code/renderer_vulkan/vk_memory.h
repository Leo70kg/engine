#ifndef VK_MEMORY_H_
#define VK_MEMORY_H_

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties);


void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access);


#endif
