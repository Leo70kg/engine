#include "tr_globals.h"
#include "vk_instance.h"
#include "vk_image.h"
#include "vk_screenshot.h"

#include "R_ImageProcess.h"

#include "../renderercommon/ref_import.h"

typedef struct {
	int commandId;
	int x;
	int y;
	int width;
	int height;
	char *fileName;
	qboolean jpeg;
} screenshotCommand_t;

typedef struct {
	int				commandId;
	int				width;
	int				height;
	unsigned char*  captureBuffer;
	unsigned char*  encodeBuffer;
	VkBool32        motionJpeg;
} videoFrameCommand_t;

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
static void vk_read_pixels(unsigned char* buffer)
{

    ri.Printf(PRINT_ALL, " vk_read_pixels() \n\n");

	qvkDeviceWaitIdle(vk.device);

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	VkImageCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	desc.extent.width = glConfig.vidWidth;
	desc.extent.height = glConfig.vidHeight;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
	VK_CHECK(qvkCreateImage(vk.device, &desc, NULL, &image));

	VkMemoryRequirements memory_requirements;
	qvkGetImageMemoryRequirements(vk.device, image, &memory_requirements);
	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
    VkDeviceMemory memory;
	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &memory));
	VK_CHECK(qvkBindImageMemory(vk.device, image, memory, 0));

  
    {

        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = NULL;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmdBuf;
        VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &cmdBuf));

        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = NULL;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = NULL;

        VK_CHECK(qvkBeginCommandBuffer(cmdBuf, &begin_info));

        record_image_layout_transition(cmdBuf, 
                vk.swapchain_images_array[vk.swapchain_image_index], 
                VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_MEMORY_READ_BIT, 
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_READ_BIT, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        record_image_layout_transition(cmdBuf, 
                image, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        VK_CHECK(qvkEndCommandBuffer(cmdBuf));

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmdBuf;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;

        VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(qvkQueueWaitIdle(vk.queue));
        qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &cmdBuf);
    }


    //////////////////////////////////////////////////////////////////////
	
    // Check if we can use vkCmdBlitImage for the given 
    // source and destination image formats.
	VkBool32 blit_enabled = 1;
	{
		VkFormatProperties formatProps;
		qvkGetPhysicalDeviceFormatProperties(
                vk.physical_device, vk.surface_format.format, &formatProps );

		if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0)
			blit_enabled = 0;

		qvkGetPhysicalDeviceFormatProperties(
                vk.physical_device, VK_FORMAT_R8G8B8A8_UNORM, &formatProps );

		if ((formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0)
			blit_enabled = 0;
	}

	if (blit_enabled)
    {
        ri.Printf(PRINT_ALL, " blit_enabled \n\n");

        VkCommandBufferAllocateInfo alloc_info;
    	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    	alloc_info.pNext = NULL;
    	alloc_info.commandPool = vk.command_pool;
    	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    	alloc_info.commandBufferCount = 1;

    	VkCommandBuffer command_buffer;
    	VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

    	VkCommandBufferBeginInfo begin_info;
    	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    	begin_info.pNext = NULL;
    	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    	begin_info.pInheritanceInfo = NULL;

    	VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));
        //	recorder(command_buffer);
        {
            VkImageBlit region;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;

            region.srcOffsets[0].x = 0;
            region.srcOffsets[0].y = 0;
            region.srcOffsets[0].z = 0;

            region.srcOffsets[1].x = glConfig.vidWidth;
            region.srcOffsets[1].y = glConfig.vidHeight;
            region.srcOffsets[1].z = 1;

            region.dstSubresource = region.srcSubresource;
            region.dstOffsets[0] = region.srcOffsets[0];
            region.dstOffsets[1] = region.srcOffsets[1];

            qvkCmdBlitImage(command_buffer,
                    vk.swapchain_images_array[vk.swapchain_image_index], 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                    VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
        }

	    VK_CHECK(qvkEndCommandBuffer(command_buffer));

    	VkSubmitInfo submit_info;
    	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    	submit_info.pNext = NULL;
    	submit_info.waitSemaphoreCount = 0;
    	submit_info.pWaitSemaphores = NULL;
    	submit_info.pWaitDstStageMask = NULL;
    	submit_info.commandBufferCount = 1;
    	submit_info.pCommandBuffers = &command_buffer;
    	submit_info.signalSemaphoreCount = 0;
    	submit_info.pSignalSemaphores = NULL;

    	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
    	VK_CHECK(qvkQueueWaitIdle(vk.queue));
    	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);   
	}
    else
    {


	    VkCommandBufferAllocateInfo alloc_info;
    	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    	alloc_info.pNext = NULL;
    	alloc_info.commandPool = vk.command_pool;
    	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    	alloc_info.commandBufferCount = 1;

	    VkCommandBuffer command_buffer;
    	VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

	    VkCommandBufferBeginInfo begin_info;
    	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    	begin_info.pNext = NULL;
    	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    	begin_info.pInheritanceInfo = NULL;

	    VK_CHECK(qvkBeginCommandBuffer(command_buffer, &begin_info));
	    {
            ////   recorder(command_buffer);
            VkImageCopy region;
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;
            region.srcOffset.x = 0;
            region.srcOffset.y = 0;
            region.srcOffset.z = 0;
            region.dstSubresource = region.srcSubresource;
            region.dstOffset = region.srcOffset;
            region.extent.width = glConfig.vidWidth;
            region.extent.height = glConfig.vidHeight;
            region.extent.depth = 1;

            qvkCmdCopyImage(command_buffer, 
                    vk.swapchain_images_array[vk.swapchain_image_index], 
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		
        }
        VK_CHECK(qvkEndCommandBuffer(command_buffer));

	    VkSubmitInfo submit_info;
    	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    	submit_info.pNext = NULL;
    	submit_info.waitSemaphoreCount = 0;
    	submit_info.pWaitSemaphores = NULL;
    	submit_info.pWaitDstStageMask = NULL;
    	submit_info.commandBufferCount = 1;
    	submit_info.pCommandBuffers = &command_buffer;
    	submit_info.signalSemaphoreCount = 0;
    	submit_info.pSignalSemaphores = NULL;

    	VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(qvkQueueWaitIdle(vk.queue));
    	qvkFreeCommandBuffers(vk.device, vk.command_pool, 1, &command_buffer);
	}



	// Copy data from destination image to memory buffer.
	VkImageSubresource subresource;
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;
	VkSubresourceLayout layout;
	qvkGetImageSubresourceLayout(vk.device, image, &subresource, &layout);


    // Memory objects created with vkAllocateMemory are not directly host accessible.
    // Memory objects created with the memory property 
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT are considered mappable. 
    // Memory objects must be mappable in order to be successfully 
    // mapped on the host. 
    // To retrieve a host virtual address pointer to 
    // a region of a mappable memory object
    unsigned char* data;
    VK_CHECK(qvkMapMemory(vk.device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&data));
	data += layout.offset;

	unsigned char* buffer_ptr = buffer + glConfig.vidWidth * (glConfig.vidHeight - 1) * 4;
	
    int j = 0;
    for (j = 0; j < glConfig.vidHeight; j++)
    {
		memcpy(buffer_ptr, data, glConfig.vidWidth * 4);
		buffer_ptr -= glConfig.vidWidth * 4;
		data += layout.rowPitch;
	}


	if (!blit_enabled)
    {
        ri.Printf(PRINT_ALL, "blit_enabled = 0 \n\n");

		//auto fmt = vk.surface_format.format;
		VkBool32 swizzle_components = 
            (vk.surface_format.format == VK_FORMAT_B8G8R8A8_SRGB ||
             vk.surface_format.format == VK_FORMAT_B8G8R8A8_UNORM ||
             vk.surface_format.format == VK_FORMAT_B8G8R8A8_SNORM);
        
		if (swizzle_components)
        {
			buffer_ptr = buffer;
            unsigned int i = 0;
            unsigned int pixels = glConfig.vidWidth * glConfig.vidHeight;
			for (i = 0; i < pixels; i++)
            {
				unsigned char tmp = buffer_ptr[0];
				buffer_ptr[0] = buffer_ptr[2];
				buffer_ptr[2] = tmp;
				buffer_ptr += 4;
			}
		}
	}
	qvkUnmapMemory(vk.device, memory);
	qvkFreeMemory(vk.device, memory, NULL);

    qvkDestroyImage(vk.device, image, NULL);
}


static void RB_TakeScreenshot( int width, int height, char *fileName )
{

    const unsigned int cnPixels = glConfig.vidWidth * glConfig.vidHeight;

	unsigned char* const buffer = (unsigned char*) malloc ( cnPixels * 3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size


    {
        unsigned char* const pImg = (unsigned char*) malloc( cnPixels * 4);

        vk_read_pixels(pImg);

        unsigned char* buffer_ptr = buffer + 18;
        const unsigned char* buffer2_ptr = pImg;

        unsigned int i;
        for (i = 0; i < cnPixels; i++)
        {
            buffer_ptr[0] = buffer2_ptr[0];
            buffer_ptr[1] = buffer2_ptr[1];
            buffer_ptr[2] = buffer2_ptr[2];
            buffer_ptr += 3;
            buffer2_ptr += 4;
        }
        
        free(pImg);
    }

    {
        unsigned int i;
        // swap rgb to bgr
        unsigned int const c = 18 + width * height * 3;
        for (i=18 ; i<c ; i+=3)
        {
            unsigned char temp = buffer[i];
            buffer[i] = buffer[i+2];
            buffer[i+2] = temp;
        }

        ri.FS_WriteFile( fileName, buffer, c );

    }

	
	free( buffer );
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

    strncpy(fileName, name, sizeof(fileName));

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
	
	RB_TakeScreenshot( cmd->width, cmd->height, cmd->fileName);
	
	return (const void *)(cmd + 1);	
}


void RE_TakeVideoFrame( int width, int height, unsigned char *captureBuffer, unsigned char *encodeBuffer, qboolean motionJpeg )
{
	if( !tr.registered ) {
		return;
	}

	videoFrameCommand_t	* cmd = R_GetCommandBuffer( sizeof( *cmd ) );
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
