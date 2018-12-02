#include "tr_local.h"
#include "R_LightScaleTexture.h"
#include "tr_globals.h"
#include "tr_cvar.h"

static unsigned char s_intensitytable[256];
static unsigned char s_gammatable[256];

void R_resetGammaIntensityTable()
{
    int i = 0;
    for (i = 0; i < 255; i++)
        s_intensitytable[i] = s_gammatable[i] = i;

}

void R_GammaCorrect(unsigned char* buffer, int bufSize)
{
	int i;

	for( i = 0; i < bufSize; i++ )
    {
		buffer[i] = s_gammatable[buffer[i]];
	}
}


void R_SetColorMappings( void )
{
	int		i, j;
	float	g;
	int		inf;
	int		shift;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;
	if ( !glConfig.deviceSupportsGamma ) {
		tr.overbrightBits = 0;		// need hardware gamma for overbright
	}

	// never overbright in windowed mode
	if ( !glConfig.isFullscreen ) 
	{
		tr.overbrightBits = 0;
	}

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit
	if ( glConfig.colorBits > 16 ) {
		if ( tr.overbrightBits > 2 ) {
			tr.overbrightBits = 2;
		}
	} else {
		if ( tr.overbrightBits > 1 ) {
			tr.overbrightBits = 1;
		}
	}
	if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;


	if ( r_intensity->value <= 1 ) {
		ri.Cvar_Set( "r_intensity", "1" );
	}

	if ( r_gamma->value < 0.5f ) {
		ri.Cvar_Set( "r_gamma", "0.5" );
	} else if ( r_gamma->value > 3.0f ) {
		ri.Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;

	shift = tr.overbrightBits;


	for ( i = 0; i < 256; i++ ) {
		if ( g == 1 ) {
			inf = i;
		} else {
			inf = 255 * pow ( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
        else if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}



	for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if ( glConfig.deviceSupportsGamma )
	{
		VKimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
	}
}



/*
================
Scale up the pixel values in a texture to increase the
lighting range
================
*/
void R_LightScaleTexture (unsigned char*in, int inwidth, int inheight, int only_gamma )
{
    int	i;
	int N = inwidth*inheight;
	
    if ( only_gamma )
	{
		if ( !glConfig.deviceSupportsGamma )
		{
			for (i=0; i < N; )
			{
				in[i] = s_gammatable[in[i]];
                i++;
				in[i] = s_gammatable[in[i]];
                i++;
				in[i] = s_gammatable[in[i]];
                i = i + 2;
			}
		}
	}
	else
	{
		if ( glConfig.deviceSupportsGamma )
		{
			for (i=0; i<N; )
			{
				in[i] = s_intensitytable[in[i]];
				i++;
				in[i] = s_intensitytable[in[i]];
				i++;
				in[i] = s_intensitytable[in[i]];
				i = i + 2;
			}
		}
		else
		{
			for (i=0; i<N; )
			{
				in[i] = s_gammatable[s_intensitytable[in[i]]];
				i++;
				in[i] = s_gammatable[s_intensitytable[in[i]]];
				i++;
				in[i] = s_gammatable[s_intensitytable[in[i]]];
				i=i+2;
			}
		}
	}
}
