#include "qvk.h"
#include "tr_local.h"


static void allocate_and_bind_image_memory(VkImage image)
{
	VkMemoryRequirements memory_requirements;
	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);

	if (memory_requirements.size > IMAGE_CHUNK_SIZE) {
		ri.Error(ERR_FATAL, "Vulkan: could not allocate memory, image is too large.");
	}

	struct Chunk* chunk = NULL;

	// Try to find an existing chunk of sufficient capacity.
	long mask = ~(memory_requirements.alignment - 1);
	for (int i = 0; i < vk_world.num_image_chunks; i++) {
		// ensure that memory region has proper alignment
		VkDeviceSize offset = (vk_world.image_chunks[i].used + memory_requirements.alignment - 1) & mask;

		if (offset + memory_requirements.size <= IMAGE_CHUNK_SIZE) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == NULL) {
		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS) {
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached");
		}

		VkMemoryAllocateInfo alloc_info;
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = IMAGE_CHUNK_SIZE;
		alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceMemory memory;
		VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		vk_world.num_image_chunks++;
		chunk->memory = memory;
		chunk->used = memory_requirements.size;
	}

	VK_CHECK(qvkBindImageMemory(vk.device, image, chunk->memory, chunk->used - memory_requirements.size));
}



struct Vk_Image vk_create_image(int width, int height, VkFormat format, int mip_levels, qboolean repeat_texture)
{
	struct Vk_Image image;

	// create image
	{
		VkImageCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &image.handle));
		allocate_and_bind_image_memory(image.handle);
	}

	// create image view
	{
		VkImageViewCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image.handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;
		VK_CHECK(qvkCreateImageView(vk.device, &desc, NULL, &image.view));
	}

	// create associated descriptor set
	{
		VkDescriptorSetAllocateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout;
		VK_CHECK(qvkAllocateDescriptorSets(vk.device, &desc, &image.descriptor_set));

		vk_update_descriptor_set(image.descriptor_set, image.view, mip_levels > 1, repeat_texture);
		vk_world.current_descriptor_sets[glState.currenttmu] = image.descriptor_set;
	}

	return image;
}
