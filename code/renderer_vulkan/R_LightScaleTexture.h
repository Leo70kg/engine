#ifndef R_LIGHT_SCALE_TEXTURE_H_
#define R_LIGHT_SCALE_TEXTURE_H_

void R_resetGammaIntensityTable(void);

void R_SetColorMappings( void );
void R_GammaCorrect(unsigned char* buffer, int bufSize);


void R_LightScaleTexture (unsigned char* const in, int inwidth, int inheight, int only_gamma, unsigned char* const dst);
#endif
