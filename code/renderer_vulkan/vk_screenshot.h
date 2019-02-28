#ifndef VK_SCREENSHOT_H_
#define VK_SCREENSHOT_H_

#include "tr_cmds.h"

void R_ScreenShotJPEG_f(void);
void R_ScreenShot_f( void );


void RB_TakeScreenshot( int width, int height, char *fileName, VkBool32 isJpeg);

void RB_TakeVideoFrameCmd( const videoFrameCommand_t * const cmd );

void RE_TakeVideoFrame( int width, int height, unsigned char *captureBuffer, unsigned char *encodeBuffer, qboolean motionJpeg );
#endif
