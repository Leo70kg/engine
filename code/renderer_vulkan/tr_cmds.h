#ifndef TR_CMD_H_
#define TR_CMD_H_


/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x40000

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} renderCommandList_t;

// 20 ?
typedef struct {
	int		commandId;
	float	color[4];
} setColorCommand_t;

// 4
typedef struct {
	int		commandId;
} drawBufferCommand_t;


// 4
typedef struct {
	int		commandId;
} swapBuffersCommand_t;


// 4
typedef struct {
	int		commandId;
} endFrameCommand_t;


// 48
typedef struct {
	int		commandId;
	shader_t* shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
} stretchPicCommand_t;


//
typedef struct {
	int		commandId;
    int		numDrawSurfs;
    drawSurf_t* drawSurfs;

	trRefdef_t	refdef;
	viewParms_t	viewParms;
} drawSurfsCommand_t;


// 4 ?
typedef enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_SCREENSHOT,
    RC_VIDEOFRAME
} renderCommand_t;


// 32 
typedef struct {
	int commandId;
	int x;
	int y;
	int width;
	int height;
    VkBool32 jpeg;
	char fileName[64];
} screenshotCommand_t;


// 32
typedef struct {
	int				commandId;
	int				width;
	int				height;
    VkBool32        motionJpeg;
	unsigned char*  captureBuffer;
	unsigned char*  encodeBuffer;
} videoFrameCommand_t;



void* R_GetCommandBuffer( unsigned int bytes );
void FixRenderCommandList( int newShader );


void R_TakeScreenshot( int x, int y, int width, int height, char *name, qboolean jpeg );

#endif
