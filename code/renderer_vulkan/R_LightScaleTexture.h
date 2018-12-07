#ifndef R_LIGHT_SCALE_TEXTURE_H_
#define R_LIGHT_SCALE_TEXTURE_H_

void R_resetGammaIntensityTable(void);

void R_SetColorMappings( void );
void R_GammaCorrect(unsigned char* buffer, int bufSize);


void R_LightScaleTexture (unsigned char* in, unsigned int nBytes, int only_gamma );
#endif
