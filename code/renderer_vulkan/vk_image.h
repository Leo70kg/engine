#ifndef VK_IMAGE_H_
#define VK_IMAGE_H_

uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);

void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkAccessFlags src_access_flags, VkImageLayout old_layout, VkAccessFlags dst_access_flags, VkImageLayout new_layout);


void vk_destroyImageRes(void);

image_t* R_FindImageFile(const char *name, VkBool32 mipmap,	VkBool32 allowPicmip, int glWrapClampMode);

image_t* R_CreateImage( const char *name, unsigned char* pic, uint32_t width, uint32_t height,
						VkBool32 mipmap, VkBool32 allowPicmip, int glWrapClampMode );


void R_LoadImage(const char *name, unsigned char **pic, uint32_t* width, uint32_t* height );
void R_LoadImage2(const char *name, unsigned char **pic, uint32_t* width, uint32_t* height );


void R_CreateBuiltinImages(void);


VkDescriptorSet* getCurDescriptorSetsPtr(void);


void GL_Bind( image_t *image );

#endif
