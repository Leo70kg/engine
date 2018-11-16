#include "tr_local.h"
#include "qvk.h"
#include "image_sampler.h"

//static cvar_t* r_textureMode;


#ifndef GL_NEAREST
#define GL_NEAREST				0x2600
#endif

#ifndef GL_LINEAR
#define GL_LINEAR				0x2601
#endif

#ifndef GL_NEAREST_MIPMAP_NEAREST
#define GL_NEAREST_MIPMAP_NEAREST		0x2700
#endif

#ifndef GL_NEAREST_MIPMAP_LINEAR
#define GL_NEAREST_MIPMAP_LINEAR		0x2702
#endif

#ifndef GL_LINEAR_MIPMAP_NEAREST
#define GL_LINEAR_MIPMAP_NEAREST		0x2701
#endif

#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR			0x2703
#endif

struct Vk_Sampler_Def
{
	VkBool32 repeat_texture; // clamp/repeat texture addressing mode
    VkBool32 mipmap;
};

/*
  The maximum number of sampler objects which can be simultaneously created on a device
  is implementation-dependent and specified by the maxSamplerAllocationCount member of
  the VkPhysicalDeviceLimits structure. If maxSamplerAllocationCount is exceeded, 
  vkCreateSampler will return VK_ERROR_TOO_MANY_OBJECTS.
*/


#define MAX_VK_SAMPLERS     32
static struct Vk_Sampler_Def s_SamplerDefs[MAX_VK_SAMPLERS] = {0};

static int s_NumSamplers = 0;
static VkSampler s_ImgSamplers[MAX_VK_SAMPLERS] = {0};



void vk_free_sampler(void)
{
    int i = 0;
    for (i = 0; i < s_NumSamplers; i++)
    {
        if(s_ImgSamplers[i] != VK_NULL_HANDLE)
        {
		    qvkDestroySampler(vk.device, s_ImgSamplers[i], NULL);
            s_ImgSamplers[i] = VK_NULL_HANDLE;
        }

        memset(&s_SamplerDefs[i], 0, sizeof(struct Vk_Sampler_Def));
    }

    s_NumSamplers = 0;
}


VkSampler vk_find_sampler( VkBool32 isMipmap, VkBool32 isRepeatTexture )
{

	// Look for sampler among existing samplers.
	int i;
    for (i = 0; i < s_NumSamplers; i++)
    {
		if (( s_SamplerDefs[i].repeat_texture == isRepeatTexture ) && ( s_SamplerDefs[i].mipmap == isMipmap ))
        {
			return s_ImgSamplers[i];
		}
	}


	struct Vk_Sampler_Def curSamplerDef;

	curSamplerDef.repeat_texture = isRepeatTexture;
	curSamplerDef.mipmap = isMipmap;
	
	s_SamplerDefs[i] = curSamplerDef;


	VkSamplerAddressMode address_mode = isRepeatTexture ?
        VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter mag_filter = VK_FILTER_LINEAR;
//	VkFilter mag_filter = VK_FILTER_NEAREST;

    VkFilter min_filter = VK_FILTER_LINEAR;
    // VkFilter min_filter = VK_FILTER_NEAREST;

    
    //used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter    
    VkBool32 max_lod_0_25 = 0;

    VkSamplerMipmapMode mipmap_mode;
    if (isMipmap)
    {
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        max_lod_0_25 = 0;
    }
    else
    {
        mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        max_lod_0_25 = 1;
    }
    //VK_SAMPLER_MIPMAP_MODE_LINEAR;


	VkSamplerCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = 0.0f;
	desc.anisotropyEnable = VK_FALSE;
	desc.maxAnisotropy = 1;
	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = max_lod_0_25 ? 0.25f : 12.0f;
	desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VkSampler sampler;
	VK_CHECK(qvkCreateSampler(vk.device, &desc, NULL, &sampler));


	s_ImgSamplers[s_NumSamplers++] = sampler;
	if (s_NumSamplers >= MAX_VK_SAMPLERS)
    {
		ri.Error(ERR_DROP, "MAX_VK_SAMPLERS hit\n");
	}
	return sampler;
}



typedef struct {
	char *name;
	int	minimize, maximize;
} textureMode_t;

const static textureMode_t texModes[] = {
    {"GL_NEAREST", GL_NEAREST, GL_NEAREST},
    {"GL_LINEAR", GL_LINEAR, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
    {"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
    {"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};


void vk_set_sampler(int m)
{
	if ( m >= 6 ) {
		ri.Printf (PRINT_ALL, "bad filter name\n");
		return;
	}
    ri.Cvar_Set( "r_textureMode", texModes[m].name);
/*
	for ( i=0; i< 6; i++ )
	{
		if ( !Q_stricmp( modes[i].name, r_textureMode->string ) ) {
			break;
		}
	}
*/
}
