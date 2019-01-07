#ifndef R_IMAGE_PROCESS_H_
#define R_IMAHE_PROCESS_H_


void R_resetGammaIntensityTable(void);
void R_SetColorMappings( void );

//void R_GammaCorrect(unsigned char* buffer, const unsigned int Size);



struct Image_Upload_Data
{
	unsigned char* buffer;
	uint32_t buffer_size;
	uint32_t mip_levels;
	uint32_t base_level_width;
	uint32_t base_level_height;
};

void generate_image_upload_data(const char *name, struct Image_Upload_Data* upload_data, 
        unsigned char* data, const uint32_t width, const uint32_t height, VkBool32 mipmap, VkBool32 picmip);

#endif
