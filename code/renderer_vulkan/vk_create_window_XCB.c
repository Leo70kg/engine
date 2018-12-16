#include "VKimpl.h"

#define VK_USE_PLATFORM_XCB_KHR

#include <vulkan/vk_sdk_platform.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <vulkan/vulkan_xcb.h>

#include "tr_local.h"
#include "qvk.h"
#include "vk_instance.h"
#include "tr_displayResolution.h"


#include "../qcommon/sys_loadlib.h"

// Allow a maximum of two outstanding presentation operations.
#define FRAME_LAG 2


#if defined(VK_USE_PLATFORM_WIN32_KHR)
#define APP_NAME_STR_LEN 80
    static HINSTANCE vk_library_handle; // HINSTANCE for the Vulkan library
    HINSTANCE connection;         // hInstance - Windows Instance
    char name[APP_NAME_STR_LEN];  // Name to put on the window/icon
    HWND window;                  // hWnd - window handle
    POINT minsize;                // minimum window size
    static PFN_vkCreateWin32SurfaceKHR qvkCreateWin32SurfaceKHR;

#elif defined(VK_USE_PLATFORM_XCB_KHR)
    static void* vk_library_handle; // instance of Vulkan library
    
    static xcb_connection_t *connection;
    
    // In the X Window System, a window is characterized by an Id.
    // So, in XCB, typedef uint32_t xcb_window_t
    typedef uint32_t xcb_window_t;
    typedef uint32_t xcb_gcontext_t;

    static xcb_window_t window;
    static xcb_screen_t *screen;
    
    static PFN_vkCreateXcbSurfaceKHR qvkCreateXcbSurfaceKHR;


    static unsigned int GetDesktopWidth(void)
    {
        // hardcode now;
        return screen->width_in_pixels;
    }

    static unsigned int GetDesktopHeight(void)
    {
        // hardcode now;
        return screen->height_in_pixels;
    }


#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *window;
    struct wl_shell *shell;
    struct wl_shell_surface *shell_surface;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
    void *window;
#endif



static void vk_resize( void )
{
/*
    // Don't react to resize until after first initialization.
    if (!demo->prepared) {
        if (demo->is_minimized) {
            demo_prepare(demo);
        }
        return;
    }
    // In order to properly resize the window, we must re-create the swapchain
    // AND redo the command buffers, etc.
    //
    // First, perform part of the demo_cleanup() function:
    demo->prepared = false;
    vkDeviceWaitIdle(demo->device);

    for (i = 0; i < demo->swapchainImageCount; i++) {
        vkDestroyFramebuffer(demo->device, demo->swapchain_image_resources[i].framebuffer, NULL);
    }
    vkDestroyDescriptorPool(demo->device, demo->desc_pool, NULL);

    vkDestroyPipeline(demo->device, demo->pipeline, NULL);
    vkDestroyPipelineCache(demo->device, demo->pipelineCache, NULL);
    vkDestroyRenderPass(demo->device, demo->render_pass, NULL);
    vkDestroyPipelineLayout(demo->device, demo->pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(demo->device, demo->desc_layout, NULL);

    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        vkDestroyImageView(demo->device, demo->textures[i].view, NULL);
        vkDestroyImage(demo->device, demo->textures[i].image, NULL);
        vkFreeMemory(demo->device, demo->textures[i].mem, NULL);
        vkDestroySampler(demo->device, demo->textures[i].sampler, NULL);
    }

    vkDestroyImageView(demo->device, demo->depth.view, NULL);
    vkDestroyImage(demo->device, demo->depth.image, NULL);
    vkFreeMemory(demo->device, demo->depth.mem, NULL);

    for (i = 0; i < demo->swapchainImageCount; i++) {
        vkDestroyImageView(demo->device, demo->swapchain_image_resources[i].view, NULL);
        vkFreeCommandBuffers(demo->device, demo->cmd_pool, 1, &demo->swapchain_image_resources[i].cmd);
        vkDestroyBuffer(demo->device, demo->swapchain_image_resources[i].uniform_buffer, NULL);
        vkFreeMemory(demo->device, demo->swapchain_image_resources[i].uniform_memory, NULL);
    }
    vkDestroyCommandPool(demo->device, demo->cmd_pool, NULL);
    demo->cmd_pool = VK_NULL_HANDLE;
    if (demo->separate_present_queue) {
        vkDestroyCommandPool(demo->device, demo->present_cmd_pool, NULL);
    }
    free(demo->swapchain_image_resources);

    // Second, re-perform the demo_prepare() function, which will re-create the
    // swapchain:
    demo_prepare(demo);
*/
}



#if defined(VK_USE_PLATFORM_WIN32_KHR)


static HWND create_main_window(int width, int height, qboolean fullscreen)
{
	//
	// register the window class if necessary
	//
	if (!s_main_window_class_registered)
	{
        cvar_t* cv = ri.Cvar_Get( "win_wndproc", "", 0 );
        WNDPROC	wndproc;
        sscanf(cv->string, "%p", (void **)&wndproc);

		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );

        wc.style         = 0;
		wc.lpfnWndProc   = wndproc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = g_wv.hInstance;
		wc.hIcon         = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = (HBRUSH) (void *)COLOR_GRAYTEXT;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = MAIN_WINDOW_CLASS_NAME;

		if ( !RegisterClass( &wc ) )
		{
			ri.Error( ERR_FATAL, "create_main_window: could not register window class" );
		}
		s_main_window_class_registered = true;
		ri.Printf( PRINT_ALL, "...registered window class\n" );
	}

	//
	// compute width and height
	//
    RECT r;
	r.left = 0;
	r.top = 0;
	r.right  = width;
	r.bottom = height;

    int	stylebits;
	if ( fullscreen )
	{
		stylebits = WS_POPUP|WS_VISIBLE|WS_SYSMENU;
	}
	else
	{
		stylebits = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
		AdjustWindowRect (&r, stylebits, FALSE);
	}

	int w = r.right - r.left;
	int h = r.bottom - r.top;

    int x, y;

	if ( fullscreen  )
	{
		x = 0;
		y = 0;
	}
	else
	{
		cvar_t* vid_xpos = ri.Cvar_Get ("vid_xpos", "", 0);
		cvar_t* vid_ypos = ri.Cvar_Get ("vid_ypos", "", 0);
		x = vid_xpos->integer;
		y = vid_ypos->integer;

		// adjust window coordinates if necessary 
		// so that the window is completely on screen
		if ( x < 0 )
			x = 0;
		if ( y < 0 )
			y = 0;

        int desktop_width = GetDesktopWidth();
        int desktop_height = GetDesktopHeight();

		if (w < desktop_width && h < desktop_height)
		{
			if ( x + w > desktop_width )
				x = ( desktop_width - w );
			if ( y + h > desktop_height )
				y = ( desktop_height - h );
		}
	}

	char window_name[1024];
	if (r_twinMode->integer == 0) {
		strcpy(window_name, MAIN_WINDOW_CLASS_NAME);
	} else {
		const char* api_name = "Vulkan";
		sprintf(window_name, "%s [%s]", MAIN_WINDOW_CLASS_NAME, api_name);
	}

	HWND hwnd = CreateWindowEx(
			0, 
			MAIN_WINDOW_CLASS_NAME,
			window_name,
			stylebits,
			x, y, w, h,
			NULL,
			NULL,
			g_wv.hInstance,
			NULL);

	if (!hwnd)
	{
		ri.Error (ERR_FATAL, "create_main_window() - Couldn't create window");
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	ri.Printf(PRINT_ALL, "...created window@%d,%d (%dx%d)\n", x, y, w, h);
    return hwnd;
}

#elif defined(VK_USE_PLATFORM_XCB_KHR)


static void CreateWindow(int width, int height, qboolean fullscreen)
{

    const xcb_setup_t *setup;
    xcb_screen_iterator_t iter;
    int screenNum;

    const char *display_envar = getenv("DISPLAY");
    if (display_envar == NULL || display_envar[0] == '\0')
    {
        ri.Error(ERR_FATAL,
            "Environment variable DISPLAY requires a valid value.");
    }

    // An X program first needs to open the connection to the X server, 
    // using xcb_connect(
    // const char *displayname, //<- if NULL, uses the DISPLAY environment variable).
    // int* screenp );  // returns the screen number of the connection;
                        // can provide NULL if you don't care.


    connection = xcb_connect(NULL, &screenNum);
    if (xcb_connection_has_error(connection) > 0)
    {
        ri.Error(ERR_FATAL,
            "Cannot find a compatible Vulkan installable client driver (ICD)");
    }

    setup = xcb_get_setup(connection);
    iter = xcb_setup_roots_iterator(setup);
    while (screenNum-- > 0)
        xcb_screen_next(&iter);

    screen = iter.data;


    uint32_t value_mask, value_list[32];


    // We first ask for a new Id for our window
    window = xcb_generate_id(connection);
    
    
    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    value_list[0] = screen->white_pixel;
    value_list[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                XCB_EVENT_MASK_ENTER_WINDOW   | XCB_EVENT_MASK_LEAVE_WINDOW   |
                XCB_EVENT_MASK_KEY_PRESS      | XCB_EVENT_MASK_KEY_RELEASE;

/*
    Then, XCB supplies the following function to create new windows:

    xcb_void_cookie_t xcb_create_window (
    xcb_connection_t *connection, // Pointer to the xcb_connection_t structure
    uint8_t depth,    // Depth of the screen
    xcb_window_t wid,    // Id of the window 
    xcb_window_t parent, // Id of the parent windows of the new window 
    int16_t x, // X position of the top-left corner of the window (in pixels)
    int16_t y, // Y position of the top-left corner of the window (in pixels)
    uint16_t width, // Width of the window (in pixels)
    uint16_t height,// Height of the window (in pixels)
    uint16_t border_width,  // Width of the window's border (in pixels)
    uint16_t _class,
    xcb_visualid_t visual,
    uint32_t value_mask,
    const uint32_t* value_list );
*/

    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root,
            0, 0, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
            screen->root_visual, value_mask, value_list);

    static const char* pVkTitle = "Vulkan Arena";
    /* Set the title of the window */
    xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,
        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen (pVkTitle), pVkTitle);

    /* set the title of the window icon */

    static const char * pIconTitle = "Open Arena (iconified)";
    
    xcb_change_property (connection, XCB_PROP_MODE_REPLACE,
        window, XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING,
            8, strlen(pIconTitle), pIconTitle);
    
    /* Magic code that will send notification when window is destroyed */
    xcb_intern_atom_cookie_t cookie 
        = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_reply_t *reply
        = xcb_intern_atom_reply(connection, cookie, 0);

    xcb_intern_atom_cookie_t cookie2
        = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
    
    xcb_intern_atom_reply_t *atom_wm_delete_window 
        = xcb_intern_atom_reply(connection, cookie2, 0);

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, 
            window, (*reply).atom, 4, 32, 1, &(*atom_wm_delete_window).atom);
    
    free(reply);

    // The fact that we created the window does not mean that
    // it will be drawn on screen. By default, newly created windows
    // are not mapped on the screen (they are invisible). In order to
    // make our window visible, we use the function xcb_map_window()

    // Mapping a window causes the window to appear on the screen, 
    // Un-mapping it causes it to be removed from the screen 
    // (although the window as a logical entity still exists). 
    // This gives the effect of making a window hidden (unmapped) 
    // and shown again (mapped). For example, if we have a dialog box
    // window in our program, instead of creating it every time the user
    // asks to open it, we can create the window once, in an un-mapped mode,
    // and when the user asks to open it, we simply map the window on the screen.
    // When the user clicked the 'OK' or 'Cancel' button, we simply un-map the window.
    // This is much faster than creating and destroying the window, 
    // however, the cost is wasted resources, both on the client side, 
    // and on the X server side. 
    xcb_map_window(connection, window);
	
    ri.Printf(PRINT_ALL, "...xcb_map_window...\n");
    
    // Force the x/y coordinates to 100,100 results are identical in consecutive
    // runs
    uint16_t mask = 0;
    mask |= XCB_CONFIG_WINDOW_X;
    mask |= XCB_CONFIG_WINDOW_Y;
    mask |= XCB_CONFIG_WINDOW_WIDTH;
    mask |= XCB_CONFIG_WINDOW_HEIGHT;

    const uint32_t coords[4] = {0, 0, width, height};
    xcb_configure_window(connection, window, mask, coords);


    // We first ask the X server to attribute an Id to our graphic context
    // Then, we set the attributes of the graphic context with xcb_create_gc

    xcb_gcontext_t  gc_black = xcb_generate_id(connection);
    uint32_t        gc_mask     = XCB_GC_FOREGROUND;
    uint32_t        gc_value[]  = { screen->black_pixel };
    xcb_create_gc (connection, gc_black, window, gc_mask, gc_value);


	// This depends on SDL_INIT_VIDEO, hence having it here
	ri.IN_Init(connection, window);
}

#endif



void vk_createWindow(void)
{

	ri.Printf(PRINT_ALL, "...vk_createWindow...\n");


    
	// Create window.
	//VKimp_SetMode(r_mode->integer, 0);
	int mode = r_mode->integer;
    qboolean fullscreen = 0;

    if (fullscreen)
    {
		ri.Printf( PRINT_ALL, "...setting fullscreen mode:");
		glConfig.vidWidth = GetDesktopWidth();
		glConfig.vidHeight = GetDesktopHeight();
		glConfig.windowAspect = 1.0f;
	}
    else
    {
		ri.Printf( PRINT_ALL, "...setting mode %d:", mode );
		R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode);

		// Ensure that window size does not exceed desktop size.
		// CreateWindow Win32 API does not allow to create windows larger than desktop.
		//int desktop_width = GetDesktopWidth();
		//int desktop_height = GetDesktopHeight();

		//if (glConfig.vidWidth > desktop_width || glConfig.vidHeight > desktop_height)
        {
			int default_mode = 5;
			ri.Printf(PRINT_WARNING, "\nMode %d specifies width that is larger than desktop width: using default mode %d\n", mode, default_mode);
			
			ri.Printf( PRINT_ALL, "...setting mode %d:", default_mode );
			
            R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, default_mode);
        }
	}
	glConfig.isFullscreen = fullscreen;
	ri.Printf( PRINT_ALL, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, fullscreen ? "FS" : "W");


	CreateWindow(glConfig.vidWidth, glConfig.vidHeight, (qboolean)r_fullscreen->integer);
//		g_wv.hWnd = g_wv.window;
//		SetForegroundWindow(g_wv.hWnd);
//		SetFocus(g_wv.hWnd);
//		WG_CheckHardwareGamma();
 
}


void vk_getInstanceProcAddrImpl(void)
{
    // Load Vulkan DLL.
#if defined( _WIN32 )
    const char* dll_name = "vulkan-1.dll";
#elif defined(MACOS_X)
    const char* dll_name = "what???";
#else
    const char* dll_name = "libvulkan.so.1";
#endif

    ri.Printf(PRINT_ALL, "...calling LoadLibrary('%s')\n", dll_name);
    vk_library_handle = Sys_LoadLibrary(dll_name);

    if (vk_library_handle == NULL) {
        ri.Error(ERR_FATAL, "VKimp_init - could not load %s\n", dll_name);
    }

    qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)Sys_LoadFunction(vk_library_handle, "vkGetInstanceProcAddr");
    if( qvkGetInstanceProcAddr == NULL)
    {
        ri.Error(ERR_FATAL, "Failed to find entrypoint vkGetInstanceProcAddr\n"); 
    }

}


void VKimp_Shutdown(void)
{
	ri.Printf(PRINT_ALL, "Shutting down Vulkan subsystem\n");

    // To close a connection, it suffices to use:
    // void xcb_disconnect (xcb_connection_t *c);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    qvkCreateWin32SurfaceKHR					= NULL;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    qvkCreateXcbSurfaceKHR                      = NULL;
#endif    
    
    //xcb_disconnect(connection);

    Sys_UnloadLibrary(vk_library_handle);

    xcb_destroy_window(connection, window);
/*  
	if (g_wv.window) {
		ri.Printf(PRINT_ALL, "...destroying Vulkan window\n");


		if (g_wv.hWnd == g_wv.window) {
			g_wv.hWnd = NULL;
		}
		g_wv.window = NULL;
	}

	if (vk_library_handle != NULL) {
		ri.Printf(PRINT_ALL, "...unloading Vulkan DLL\n");
		FreeLibrary(vk_library_handle);
		vk_library_handle = NULL;
	}
	qvkGetInstanceProcAddr = NULL;

	// For vulkan mode we still have qgl pointers initialized with placeholder values.
	// Reset them the same way as we do in opengl mode.
	QGL_Shutdown();

	WG_RestoreGamma();
*/
	memset(&glConfig, 0, sizeof(glConfig));

}


void VKimp_CreateSurface(void)
{

// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)

    qvkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)qvkGetInstanceProcAddr(vk.instance, "vkCreateWin32SurfaceKHR");
    if (qvkCreateWin32SurfaceKHR == NULL)
    {
        ri.Error(ERR_FATAL, "Failed to find entrypoint %s", "vkCreateWin32SurfaceKHR");
    }

    VkWin32SurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.hinstance = 0;
    createInfo.hwnd = window;

    qvkCreateWin32SurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);

#elif defined(VK_USE_PLATFORM_XCB_KHR)
    qvkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)qvkGetInstanceProcAddr(vk.instance, "vkCreateXcbSurfaceKHR");
    if( qvkCreateXcbSurfaceKHR == NULL)
    {
        ri.Error(ERR_FATAL, "Failed to find entrypoint qvkCreateXcbSurfaceKHR\n"); 
    }
   
    VkXcbSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.connection = connection;
    createInfo.window = window;

    qvkCreateXcbSurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.display = display;
    createInfo.surface = window;

    qvkCreateWaylandSurfaceKHR(vk.instance, &createInfo, NULL, &vk.surface);

#endif
}

static void CreateInstanceImpl(unsigned int numExt, const char* extNames[])
{
	VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "OpenArena";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "OpenArena";
	appInfo.engineVersion = 1;
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = numExt;
	instanceCreateInfo.ppEnabledExtensionNames = extNames;


#ifndef NDEBUG
	const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	instanceCreateInfo.enabledLayerCount = 1;
	instanceCreateInfo.ppEnabledLayerNames = &validation_layer_name;
#endif

    VkResult e = qvkCreateInstance(&instanceCreateInfo, NULL, &vk.instance);
    if(!e)
    {
        ri.Printf(PRINT_ALL, "---Vulkan create instance success---\n\n");
    }
    else if (e == VK_ERROR_INCOMPATIBLE_DRIVER) {
        ri.Error(ERR_FATAL, 
            "Cannot find a compatible Vulkan installable client driver (ICD).\n" );
    }
    else if (e == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        ri.Error(ERR_FATAL, "Cannot find a specified extension library.\n");
    }
    else 
    {
        ri.Error(ERR_FATAL, "%d, returned by qvkCreateInstance.\n", e);
    }
}


/*  

VkResult vkEnumerateInstanceExtensionProperties(
const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties );

pLayerName is either NULL or a pointer to a null-terminated UTF-8 string naming the layer to
retrieve extensions from.

pPropertyCount is a pointer to an integer related to the number of extension properties available
or queried, as described below.

pProperties is either NULL or a pointer to an array of VkExtensionProperties structures.

If pProperties is NULL, then the number of extensions properties available is returned in
pPropertyCount. Otherwise, pPropertyCount must point to a variable set by the user to the number of
elements in the pProperties array, and on return the variable is overwritten with the number of
structures actually written to pProperties. If pPropertyCount is less than the number of extension
properties available, at most pPropertyCount structures will be written. If pPropertyCount is smaller
than the number of extensions available, VK_INCOMPLETE will be returned instead of VK_SUCCESS, to
indicate that not all the available properties were returned.

Because the list of available layers may change externally between calls to 
vkEnumerateInstanceExtensionProperties, two calls may retrieve different results if a pLayerName is
available in one call but not in another. The extensions supported by a layer may also change
between two calls, e.g. if the layer implementation is replaced by a different version between those
calls.

*/

void VKimp_CreateInstance(void)
{
	// check extensions availability
	unsigned int instance_extension_count = 0;
    VkBool32 surfaceExtFound = 0;
    VkBool32 platformSurfaceExtFound = 0;
     
    const char* extension_names_supported[64] = {0};
    unsigned int enabled_extension_count = 0;

	VkResult result = qvkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
    if(result < 0)
	    ri.Error(ERR_FATAL, "Vulkan: error code %d returned by %s", result, "vkEnumerateInstanceExtensionProperties");

    if (instance_extension_count > 0)
    {
        VkExtensionProperties *instance_extensions = 
            (VkExtensionProperties *) malloc(sizeof(VkExtensionProperties) * instance_extension_count);
        VkResult err = qvkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, instance_extensions);
        if(err < 0)
	        ri.Error(ERR_FATAL, "Vulkan: error code %d returned by %s", err, "vkEnumerateInstanceExtensionProperties");
            
        unsigned int i = 0;

        for (i = 0; i < instance_extension_count; i++)
        {
            if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
            {
                surfaceExtFound = 1;
                extension_names_supported[enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
            }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
            if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
            {
                platformSurfaceExtFound = 1;
                extension_names_supported[enabled_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
            }
#elif defined(VK_USE_PLATFORM_XCB_KHR)
            if (!strcmp(VK_KHR_XCB_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
            {
                platformSurfaceExtFound = 1;
                extension_names_supported[enabled_extension_count++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
            }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            if (!strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName))
            {
                platformSurfaceExtFound = 1;
                extension_names_supported[enabled_extension_count++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
            }
#endif

#ifndef NDEBUG
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instance_extensions[i].extensionName))
            {
                extension_names_supported[enabled_extension_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            }
#endif
        }
            
        assert(enabled_extension_count < 64);
            
        free(instance_extensions);
    }

    if (!surfaceExtFound)
		ri.Error(ERR_FATAL, "Vulkan: required instance extension is not available: %s", "surfaceExt");
    if (!platformSurfaceExtFound)
		ri.Error(ERR_FATAL, "Vulkan: required instance extension is not available: %s", "platformSurfaceExt");

    CreateInstanceImpl(enabled_extension_count, extension_names_supported);
}


void VKimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	unsigned short int table[3][256];
	int i, j;

	for (i = 0; i < 256; i++)
	{
		table[0][i] = ( red[i] << 8 ) | red[i];
		table[1][i] = ( green[i] << 8 ) | green[i];
		table[2][i] = ( blue[i]  << 8 ) | blue[i];
	}


	// enforce constantly increasing
	for (j = 0; j < 3; j++)
	{
		for (i = 1; i < 256; i++)
		{
			if (table[j][i] < table[j][i-1])
				table[j][i] = table[j][i-1];
		}
	}

}


/*
===============
Minimize the game so that user is back at the desktop
===============
*/
void VKimp_Minimize( void )
{
    if(r_fullscreen->integer == 1)
    {
        r_fullscreen->integer = 0;
        ri.Cmd_ExecuteText (EXEC_NOW, "vid_restart\n");
        r_fullscreen->integer = 1;
    }

}

// doc

/* 
 * Once we have opened a connection to an X server, 
   we should check some basic information about it: 
   what screens it has, what is the size (width and height) of the screen,
   how many colors it supports, and so on. 
   We get such information from the xcbscreent structure:

    typedef struct {
        xcb_window_t   root;
        xcb_colormap_t default_colormap;
        uint32_t       white_pixel;
        uint32_t       black_pixel;
        uint32_t       current_input_masks;
        uint16_t       width_in_pixels;
        uint16_t       height_in_pixels;
        uint16_t       width_in_millimeters;
        uint16_t       height_in_millimeters;
        uint16_t       min_installed_maps;
        uint16_t       max_installed_maps;
        xcb_visualid_t root_visual;
        uint8_t        backing_stores;
        uint8_t        save_unders;
        uint8_t        root_depth;
        uint8_t        allowed_depths_len;
    } xcb_screen_t;

*/

/*
 * Drawing in a window can be done using various graphical functions
   (drawing pixels, lines, rectangles, etc). In order to draw in a window,
   we first need to define various general drawing parameters, what line 
   width to use, which color to draw with, etc. This is done using a 
   graphical context. A graphical context defines several attributes to be
   used with the various drawing functions. For this, we define a graphical
   context. We can use more than one graphical context with a single window, 
   in order to draw in multiple styles (different colors, line widths, etc).
   In XCB, a Graphics Context is, as a window, characterized by an Id:
*/
