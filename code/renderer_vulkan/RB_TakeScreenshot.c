#include "tr_local.h"
#include "vk_read_pixels.h"
#include "R_LightScaleTexture.h"
#include "RB_TakeScreenshot.h"
#include "tr_globals.h"

typedef struct {
	int commandId;
	int x;
	int y;
	int width;
	int height;
	char *fileName;
	qboolean jpeg;
} screenshotCommand_t;
/* 
============================================================================== 
 
						SCREEN SHOTS 

NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)
FIXME: the statics don't get a reinit between fs_game changes

============================================================================== 
*/ 



static void RB_TakeScreenshot( int x, int y, int width, int height, char *fileName )
{

	int	i, c, temp;
		
	unsigned char* buffer = (unsigned char*) ri.Hunk_AllocateTempMemory(glConfig.vidWidth*glConfig.vidHeight*3+18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size


    unsigned char* buffer2 = (unsigned char*) ri.Hunk_AllocateTempMemory(glConfig.vidWidth*glConfig.vidHeight*4);

    vk_read_pixels(buffer2);

    unsigned char* buffer_ptr = buffer + 18;
    unsigned char* buffer2_ptr = buffer2;
    for (i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++)
    {
        buffer_ptr[0] = buffer2_ptr[0];
        buffer_ptr[1] = buffer2_ptr[1];
        buffer_ptr[2] = buffer2_ptr[2];
        buffer_ptr += 3;
        buffer2_ptr += 4;
    }
    ri.Hunk_FreeTempMemory(buffer2);


	// swap rgb to bgr
	c = 18 + width * height * 3;
	for (i=18 ; i<c ; i+=3) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	// gamma correct
	if ( ( tr.overbrightBits > 0 ) && glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, glConfig.vidWidth * glConfig.vidHeight * 3 );
	}

	ri.FS_WriteFile( fileName, buffer, c );

	ri.Hunk_FreeTempMemory( buffer );
}





static void R_TakeScreenshot( int x, int y, int width, int height, char *name, qboolean jpeg )
{
	static char	fileName[MAX_OSPATH] = {0}; // bad things if two screenshots per frame?
	
    screenshotCommand_t	*cmd = (screenshotCommand_t*) R_GetCommandBuffer(sizeof(*cmd));
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SCREENSHOT;

	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	
    //Q_strncpyz( fileName, name, sizeof(fileName) );

    strcpy(fileName, name);

	cmd->fileName = fileName;
	cmd->jpeg = jpeg;
}



/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for the 
menu system, sampled down from full screen distorted images
====================
*/
static void R_LevelShot( void )
{
	char checkname[MAX_OSPATH];
	unsigned char* buffer;
	unsigned char* source;
	unsigned char* src;
	unsigned char* dst;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;
    int i = 0;
	sprintf( checkname, "levelshots/%s.tga", tr.world->baseName );

	source = (unsigned char*) ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight * 3 );

	buffer = (unsigned char*) ri.Hunk_AllocateTempMemory( 128 * 128*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

    {
        unsigned char* buffer2 = (unsigned char*) ri.Hunk_AllocateTempMemory(glConfig.vidWidth*glConfig.vidHeight*4);
        vk_read_pixels(buffer2);

        unsigned char* buffer_ptr = source;
        unsigned char* buffer2_ptr = buffer2;
        for (i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++)
        {
            buffer_ptr[0] = buffer2_ptr[0];
            buffer_ptr[1] = buffer2_ptr[1];
            buffer_ptr[2] = buffer2_ptr[2];
            buffer_ptr += 3;
            buffer2_ptr += 4;
        }
        ri.Hunk_FreeTempMemory(buffer2);
    }

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + 3 * ( glConfig.vidWidth * (int)( (y*3+yy)*yScale ) + (int)( (x*4+xx)*xScale ) );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if ( ( tr.overbrightBits > 0 ) && glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, 128 * 128 * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory( buffer );
	ri.Hunk_FreeTempMemory( source );

	ri.Printf( PRINT_ALL, "Wrote %s\n", checkname );
}

/* 
================== 
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
================== 
*/  
void R_ScreenShot_f (void)
{
	char	checkname[MAX_OSPATH];
	static	int	lastNumber = -1;
	qboolean	silent;

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) )
    {
		R_LevelShot();
		return;
	}

	if ( !strcmp( ri.Cmd_Argv(1), "silent" ) )
    {
		silent = qtrue;
	}
    else
    {
		silent = qfalse;
	}


	if ( ri.Cmd_Argc() == 2 && !silent )
    {
		// explicit filename
		snprintf( checkname, sizeof(checkname), "screenshots/%s.tga", ri.Cmd_Argv( 1 ) );
	}
    else
    {
		// scan for a free filename

		// if we have saved a previous screenshot, don't scan again, 
        // because recording demo avis can involve thousands of shots
		if ( lastNumber == -1 ) {
			lastNumber = 0;
		}
		// scan for a free number
		for ( ; lastNumber <= 9999 ; lastNumber++ )
        {
			//R_ScreenshotFilename( lastNumber, checkname );
            
            int	a,b,c,d;

            a = lastNumber / 1000;
            lastNumber -= a*1000;
            b = lastNumber / 100;
            lastNumber -= b*100;
            c = lastNumber / 10;
            lastNumber -= c*10;
            d = lastNumber;

            snprintf( checkname, sizeof(checkname), "screenshots/shot%i%i%i%i.tga", a, b, c, d );

            if (!ri.FS_FileExists( checkname ))
            {
                break; // file doesn't exist
            }
		}

		if ( lastNumber >= 9999 )
        {
			ri.Printf (PRINT_ALL, "ScreenShot: Couldn't create a file\n"); 
			return;
 		}

		lastNumber++;
	}

	R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, qfalse );

	if ( !silent ) {
		ri.Printf (PRINT_ALL, "Wrote %s\n", checkname);
	}
} 


void R_ScreenShotJPEG_f(void)
{
	char		checkname[MAX_OSPATH];
	static	int	lastNumber = -1;
	qboolean	silent;

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if ( !strcmp( ri.Cmd_Argv(1), "silent" ) ) {
		silent = qtrue;
	} else {
		silent = qfalse;
	}

	if ( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		snprintf( checkname, sizeof(checkname), "screenshots/%s.jpg", ri.Cmd_Argv( 1 ) );
	} else {
		// scan for a free filename

		// if we have saved a previous screenshot, don't scan
		// again, because recording demo avis can involve
		// thousands of shots
		if ( lastNumber == -1 ) {
			lastNumber = 0;
		}
		// scan for a free number
		for ( ; lastNumber <= 9999 ; lastNumber++ )
        {
            int	a,b,c,d;

            a = lastNumber / 1000;
            lastNumber -= a*1000;
            b = lastNumber / 100;
            lastNumber -= b*100;
            c = lastNumber / 10;
            lastNumber -= c*10;
            d = lastNumber;

            snprintf( checkname, sizeof(checkname), "screenshots/shot%i%i%i%i.jpg"
                    , a, b, c, d );

            if (!ri.FS_FileExists( checkname ))
            {
                break; // file doesn't exist
            }
		}

		if ( lastNumber == 10000 ) {
			ri.Printf (PRINT_ALL, "ScreenShot: Couldn't create a file\n"); 
			return;
 		}

		lastNumber++;
	}

	R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname, qtrue );

	if ( !silent ) {
		ri.Printf (PRINT_ALL, "Wrote %s\n", checkname);
	}
}

const void *RB_TakeScreenshotCmd( const void *data )
{
	const screenshotCommand_t* cmd = (const screenshotCommand_t *)data;
	
	RB_TakeScreenshot( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
	
	return (const void *)(cmd + 1);	
}


void RE_TakeVideoFrame( int width, int height, unsigned char *captureBuffer, unsigned char *encodeBuffer, qboolean motionJpeg )
{
	videoFrameCommand_t	*cmd;

	if( !tr.registered ) {
		return;
	}

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if( !cmd ) {
		return;
	}

	cmd->commandId = RC_VIDEOFRAME;

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}
