#ifndef RB_TAKE_SCREENSHOT_H_
#define RB_TAKE_SCREENSHOT_H_

void R_ScreenShotJPEG_f(void);
void R_ScreenShot_f( void );

const void* RB_TakeScreenshotCmd( const void *data );

typedef struct {
	int				commandId;
	int				width;
	int				height;
	unsigned char*  captureBuffer;
	unsigned char*  encodeBuffer;
	qboolean        motionJpeg;
} videoFrameCommand_t;

void RE_TakeVideoFrame( int width, int height, unsigned char *captureBuffer, unsigned char *encodeBuffer, qboolean motionJpeg );

#endif
