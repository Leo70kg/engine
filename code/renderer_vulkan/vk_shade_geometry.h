#ifndef VK_SHADE_GEOMETRY
#define VK_SHADE_GEOMETRY

enum Vk_Depth_Range {
	normal, // [0..1]
	force_zero, // [0..0]
	force_one, // [1..1]
	weapon // [0..0.3]
};


void vk_shade_geometry(VkPipeline pipeline, VkBool32 multitexture, enum Vk_Depth_Range depth_range, VkBool32 indexed);

#endif
