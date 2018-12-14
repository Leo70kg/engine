#ifndef VK_SCREENSHOT_H_
#define VK_SCREENSHOT_H_

void R_ScreenShotJPEG_f(void);
void R_ScreenShot_f( void );

const void* RB_TakeScreenshotCmd( const void *data );



void RE_TakeVideoFrame( int width, int height, unsigned char *captureBuffer, unsigned char *encodeBuffer, VkBool32 motionJpeg );

#endif
