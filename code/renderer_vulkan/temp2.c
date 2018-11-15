/*
 * =====================================================================================
 *
 *       Filename:  temp2.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2018年11月15日 23时26分42秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Sui Jingfeng (), jingfengsui@gmail.com
 *   Organization:  CASIA(2014-2017)
 *
 * =====================================================================================
 */


static void ensure_staging_buffer_allocation(VkDeviceSize size, const unsigned char* pPixels)
{

	if (s_StagingBufferSize >= size)
		return;

	if (s_StagingBuffer != VK_NULL_HANDLE)
    {
		qvkDestroyBuffer(vk.device, s_StagingBuffer, NULL);
    }

	if (s_StagingBufferMemory != VK_NULL_HANDLE)
    {
		qvkFreeMemory(vk.device, s_StagingBufferMemory, NULL);
    }
	s_StagingBufferSize = size;

	VkBufferCreateInfo buffer_desc;
	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &s_StagingBuffer));

    // To determine the memory requirements for a buffer resource

	VkMemoryRequirements memory_requirements;
	qvkGetBufferMemoryRequirements(vk.device, s_StagingBuffer, &memory_requirements);

	uint32_t memory_type = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;
	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &s_StagingBufferMemory));
	VK_CHECK(qvkBindBufferMemory(vk.device, s_StagingBuffer, s_StagingBufferMemory, 0));

	void* data;
	VK_CHECK(qvkMapMemory(vk.device, s_StagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data));
	
    s_pStagingBuffer = (unsigned char *)data;

    memcpy(s_pStagingBuffer, pPixels, size);
}

