#ifndef VK_IMAGE_H_
#define VK_IMAGE_H_

struct Image_Upload_Data
{
	unsigned char* buffer;
	int buffer_size;
	int mip_levels;
	int base_level_width;
	int base_level_height;
};

void generate_image_upload_data(struct Image_Upload_Data* upload_data, unsigned char* data,
        int width, int height, qboolean mipmap, qboolean picmip);


void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
	VkAccessFlags src_access_flags, VkImageLayout old_layout, VkAccessFlags dst_access_flags, VkImageLayout new_layout);


void qDestroyImage(void);

image_t* R_FindImageFile(const char *name, qboolean mipmap,	qboolean allowPicmip, int glWrapClampMode);
image_t *R_CreateImage( const char *name, unsigned char *pic, int width, int height,
						qboolean mipmap, qboolean allowPicmip, int glWrapClampMode );



void R_LoadImage(const char *name, unsigned char **pic, int *width, int *height );
void R_LoadImage2(const char *name, unsigned char **pic, int *width, int *height );


void R_CreateBuiltinImages(void);


VkDescriptorSet* getCurDescriptorSetsPtr(void);
#endif
