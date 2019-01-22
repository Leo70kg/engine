#ifndef TR_IMAGE_H_
#define TR_IMAGE_H_




typedef struct image_s {
	char		imgName[MAX_QPATH];		// game path, including extension
	int			width, height;				// source image
	int			uploadWidth, uploadHeight;	// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	uint32_t texnum;					// gl texture binding

	int			frameUsed;			// for texture usage in frame statistics

	int			internalFormat;
	int			TMU;				// only needed for voodoo2
    int         index;

    int			wrapClampMode;		// GL_CLAMP or GL_REPEAT, for vulkan
    qboolean    mipmap;             // for vulkan
    qboolean    allowPicmip;        // for vulkan
	struct image_s*	next;
} image_t;



#endif
