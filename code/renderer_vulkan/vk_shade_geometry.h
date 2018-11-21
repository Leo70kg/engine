#ifndef VK_SHADE_GEOMETRY
#define VK_SHADE_GEOMETRY

enum Vk_Depth_Range {
	normal, // [0..1]
	force_zero, // [0..0]
	force_one, // [1..1]
	weapon // [0..0.3]
};



#define INDEX_BUFFER_SIZE   (2 * 1024 * 1024)
#define VERTEX_CHUNK_SIZE   (512 * 1024)

#define XYZ_SIZE            (4 * VERTEX_CHUNK_SIZE)
#define COLOR_SIZE          (1 * VERTEX_CHUNK_SIZE)
#define ST0_SIZE            (2 * VERTEX_CHUNK_SIZE)
#define ST1_SIZE            (2 * VERTEX_CHUNK_SIZE)

#define XYZ_OFFSET          0
#define COLOR_OFFSET        (XYZ_OFFSET + XYZ_SIZE)
#define ST0_OFFSET          (COLOR_OFFSET + COLOR_SIZE)
#define ST1_OFFSET          (ST0_OFFSET + ST0_SIZE)

#define VERTEX_BUFFER_SIZE  (XYZ_SIZE + COLOR_SIZE + ST0_SIZE + ST1_SIZE)

VkRect2D get_scissor_rect(void);

void vk_shade_geometry(VkPipeline pipeline, VkBool32 multitexture, enum Vk_Depth_Range depth_range, VkBool32 indexed);
void vk_bind_geometry(void);
void RB_ShowImages(void);

#endif
