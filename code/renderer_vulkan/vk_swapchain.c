#include "../renderercommon/ref_import.h"
#include "tr_cvar.h"
#include "VKimpl.h"

void vk_recreateSwapChain(void)
{

    ri.Printf( PRINT_ALL, " Recreate swap chain \n");

    if( r_fullscreen->integer )
    {
        ri.Cvar_Set( "r_fullscreen", "0" );
        r_fullscreen->modified = qtrue;
    }
    
    // hasty prevent crash.

    ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");

    minimizeWindowImpl();

    // clean up the old resource 
/*        
	
    if(isWinFullscreen)
		 = (SDL_SetWindowFullscreen( window_sdl, 0 ) >= 0);

    // window mode don't need toggle, if fullscreen we need toggle it

    // obviously, the first thing we'll have to do is recreate the wwap chain itself.
    //
    //
    // The image views need to be recreated because they are based directly on
    // the swap chain images.
    //
    //
    // The render pass needs to be recreated because it depends on the swap chain images.
    //
    //
    // It is rare for the swap chain image format to change during an operation
    // like a window resize, but it should still be handled. Viewport and scissor
    // retangle size is specified during graphics pipeline creation, so the 
    // pipeline also needs to be rebuilt. 
    //
    //

    // Finally, the framebuffers and command buffers also directly depend on the swap chain images
*/
}
