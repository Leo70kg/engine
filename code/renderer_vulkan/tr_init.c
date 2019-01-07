/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "tr_globals.h"
#include "tr_displayResolution.h"

#include "VKimpl.h"
#include "vk_initialize.h"
#include "vk_screenshot.h"
#include "vk_shade_geometry.h"
#include "vk_pipelines.h"
#include "vk_image.h"
#include "vk_clear_attachments.h"
#include "R_LerpTag.h"
#include "R_ModelBounds.h"
#include "tr_model.h"
#include "tr_cvar.h"

refimport_t	ri;




static void InitRenderAPI( void )
{
	
	if ( glConfig.vidWidth == 0 )
	{
        // VULKAN
		// initialize OS specific portions of the renderer	
        vk_createWindow();
        
        vk_getProcAddress(); 
        // This function is responsible for initializing a valid Vulkan subsystem.

		vk_initialize();

	}
	
    // print info
	vulkanInfo_f();

}



/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration( void )
{
	if ( tr.registered ) {
		R_IssueRenderCommands( qfalse );
	}
}



void R_Init( void )
{	
	int i;

	ri.Printf( PRINT_ALL, "----- R_Init -----\n" );


	// clear all our internal state
	memset( &tr, 0, sizeof( tr ) );
	memset( &backEnd, 0, sizeof( backEnd ) );
	memset( &tess, 0, sizeof( tess ) );

	if ( (intptr_t)tess.xyz & 15 ) {
		ri.Printf( PRINT_ALL, "WARNING: tess.xyz not 16 byte aligned\n" );
	}


	//
	// init function tables
	//
	for ( i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		tr.squareTable[i]	= ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 )
		{
			if ( i < FUNCTABLE_SIZE / 4 )
			{
				tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
		}
	}

    R_InitDisplayResolution();

	R_InitFogTable();

	R_NoiseInit();

	R_Register();

	// make sure all the commands added here are also
	// removed in R_Shutdown
    ri.Cmd_AddCommand( "displayResoList", R_DisplayResolutionList_f );

    ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "screenshotJPEG", R_ScreenShotJPEG_f );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );

    ri.Cmd_AddCommand( "vkinfo", vulkanInfo_f );
    ri.Cmd_AddCommand( "minimize", VKimp_Minimize );

    ri.Cmd_AddCommand( "pipelineList", R_PipelineList_f );

    R_InitScene();

	InitRenderAPI();

	R_InitImages();

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	R_InitFreeType();

    ri.Printf( PRINT_ALL, "----- R_Init finished -----\n" );
}




void RE_Shutdown( qboolean destroyWindow )
{	

	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow );
    
    ri.Cmd_RemoveCommand("displayResoList");

	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshotJPEG");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("shaderlist");
	ri.Cmd_RemoveCommand("skinlist");

    ri.Cmd_RemoveCommand("minimize");
	ri.Cmd_RemoveCommand("vkinfo");

    ri.Cmd_RemoveCommand("pipelineList");

	if ( tr.registered )
    {
        R_IssueRenderCommands( qfalse );
		
        memset( tr.images, 0, sizeof( tr.images ) );

	    tr.numImages = 0;
	}


	R_DoneFreeType();

    // VULKAN
    // Releases vulkan resources allocated during program execution.
    // This effectively puts vulkan subsystem into initial state 
    // (the state we have after vk_initialize call).

    // contains vulkan resources/state, reinitialized on a map change.


    vk_destroyShaderStagePipeline();
 
    vk_destroyImageRes();

    set_depth_attachment(VK_FALSE);

    vk_resetGeometryBuffer();
    
    if (destroyWindow)
    {
        vk_shutdown();
        vk_destroyWindow();
    }

	tr.registered = qfalse;
}


void RE_BeginRegistration(glconfig_t *glconfigOut)
{
	R_Init();

	*glconfigOut = glConfig;

	R_IssuePendingRenderCommands();

	tr.viewCluster = -1; // force markleafs to regenerate

	RE_ClearScene();

	tr.registered = qtrue;

   	ri.Printf(PRINT_ALL, "RE_BeginRegistration finished.\n");
}



/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI( int apiVersion, refimport_t *rimp )
{
#else
refexport_t* GetRefAPI(int apiVersion, refimport_t *rimp)
{
#endif

	ri = *rimp;

	if( apiVersion != REF_API_VERSION )
	{
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion );
		return NULL;
	}

	static refexport_t re;
	memset(&re, 0, sizeof(re));

    
	// the RE_ functions are Renderer Entry points
	re.Shutdown = RE_Shutdown;
	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;
	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;

	re.RenderScene = RE_RenderScene;
	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
    
	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;

	return &re;
}
