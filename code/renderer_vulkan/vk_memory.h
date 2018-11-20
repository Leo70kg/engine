#ifndef VK_MEMORY_H_
#define VK_MEMORY_H_

#include "qvk.h"

unsigned int find_memory_type(VkPhysicalDevice physical_device, unsigned int memory_type_bits, VkMemoryPropertyFlags properties);


void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
		VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
		VkAccessFlags src_access, VkAccessFlags dst_access);

#endif
