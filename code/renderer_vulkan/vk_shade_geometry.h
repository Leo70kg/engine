#ifndef VK_SHADE_GEOMETRY
#define VK_SHADE_GEOMETRY

enum Vk_Depth_Range {
	DEPTH_RANGE_NORMAL, // [0..1]
	DEPTH_RANGE_ZERO, // [0..0]
	DEPTH_RANGE_ONE, // [1..1]
	DEPTH_RANGE_WEAPON // [0..0.3]
};


void get_modelview_matrix(float mv[16]);
void set_modelview_matrix(const float mv[16]);
void reset_modelview_matrix(void);
void R_SetupProjection( float pMatProj[16] );

VkRect2D get_scissor_rect(void);

void vk_shade_geometry(VkPipeline pipeline, VkBool32 multitexture, enum Vk_Depth_Range depth_range, VkBool32 indexed);
void vk_bind_geometry(void);
void vk_resetGeometryBuffer(void);
void vk_createGeometryBuffers(void);

VkBuffer vk_getIndexBuffer(void);
void vk_destroy_shading_data(void);

void updateCurDescriptor( VkDescriptorSet curDesSet, uint32_t tmu);

#endif
