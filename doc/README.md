# Quake 3: Arena Rendering Architecture

by Brian Hook, id Software Presentation at GDC 1999


## Introduction ##

Note: this is an ASCII dump of the http://www.gdconf.com/1999/library/5301.ppt powerpoint file,
This will give you bare bone scratchpad notes as one might have taken
during the session, with bits and pieces missing or out of order.

## Portability and OS Support ##
   Win9x
   WinNT (AXP & x86)
   MacOS
   Linux

New renderer with material based shaders (built over OpenGL1.1 Fixed Pipeline).
Window system stuff is abstracted through an intermediate layer.
 

## Incremental improvement on graphics technology ##

    Volumetric fog
    Portals/mirrors
    Wall marks, shadows, light flares, etc.
    Environment mapping
    Shader architecture allows us to do cool general effects 
    specified in a text file, e.g. fire and quad
    Better lighting
    Specular lighting
    Dynamic character lighting
    Tagged model system
    Optimized for OpenGL
 


* Volumetric fog
    distance based fog sucks
    constant density or gradient
    fog volumes are defined by brushes
    triangles inside a fog volume are rendered with another pass, 
    with alpha equal to density computed as the distance
    from the viewer to the vertex through the fog volume

advantages of our technique
    Allows true volumetric fog

disadvantages
    T-junctions introduced at the boundary between the fog brush 
     and the non-fog brush due to tessellation of the triangles 
    inside the fog-brush.
    Triangle interpolation artifacts
    Excessive triangle count due to the heavy tessellation

## Portals/mirrors ##

    basically equivalent, only difference is location of the virtual viewpoint
    only a single portal/mirror is allowed at once to avoid infinite recursion
    insert PVS sample point at mirror location

## Environment mapping ##

    Wall marks

    Volumetric shadows

    Problems
       Shadows can cast through surfaces
       Expensive since you have to determine silhouette edges
         (connectivity can be precomputed however)
       Expensive since it requires a LOT of overdraw, both in 
         the quads and the final screen blend
       exhibits artifacts such as inconsistency with world 
         geometry or Gouraud shading on models


## Rendering primitives ##

   Quadratic Bezier patches
      Tessellated at load time to arbitrary detail level
      Row/column dropping for dynamic load balancing
      Simpler to manipulate than cubic Bezier patches
      Artifacts not very noticeable   

## MD3 (arbitrary triangle meshes) ##
      Multipart player models consisting of connect animated 
      vertex meshes created in 3DSMAX
      Post processed by Q3DATA into MD3 format
      Simple popping LOD scheme
      Based on projected sphere size
      LOD bias capability
      No noticeable artifacts
      Suitable technological progress given our time frame
      Spurred by need for convincing clouds and environment

## standard Q2 sky box ##
      projection of clouds onto hemisphere, multiple layers possible
      tessellated output feeds into standard shader pipeline

## Lightmaps ##
      Covers same world area as Q2, 1 lightmap texel covers 2 sq. ft., 
      which corresponds to one 32x32 texture block Generated using direct lighting, 
      not radiosity.
      
      Diffuse lightmaps blended using src*dst.
      
      Dynamic lights handled through three dynamically modified lightmaps
      uploaded with glTexSubImage2D, performance gain from using subimage.

## Specular lighting ##
      specular lighting is simply a hacked form of dynamic environment mapping
      specularity encoded in alpha channel of texture (mono-specular materials)
      color iterator stores the generated specular light value in iterated alpha
      walls render lightmap then base texture 
       using src*dst + dst*src.alpha
      models render Gouraud only, then base texture 
       using src*dst + dst*src.alpha

## Character lighting ##
    * Overbrightening
      
      lighting program assumes a dynamic range 2x than normally exists
      during rendering we set the hardware gamma ramp so that the back half of it is saturated to identity
      
      net effect is that all textures are doubled in brightness but only exhibit saturation artifacts if they exceed 50% intensity
      
      requires Get/SetDeviceGammaRamp
      
      not available on NT4
      
      requires 3Dfx extension on Voodoo2
      
      we gain dynamic range in exchange for lower precision

    * Cheezy shadows
      project a polygon straight down
      alpha determined by height of object
      basically just a dynamic wall mark

    * Sunlight


## Shader Architecture ##

actually materials
    many special effects done with very little coding
    descriptions in text (.shader) files
    general information stored in the material definition body
  
    sound type
    editor image
    vertex deformation information
    lighting information
    misc other material global information
    tessellation density (for brushes only)
    sort bias

    multiple stages can specify:
    texture map (name or $lightmap)
    texcoord source
    environment mapping
    texture coordinates
    lightmap coordinates

    alpha and color sources
    identity
    waveform
    client game
    diffuse/specular lighting
    alpha and color modifiers
    unused right now

    texture coordinate modifiers
    rotate (degs/second)
    clamp enable/disable
    scroll (units/second per S & T)
    turbulence
    scale (waveform)
    arbitrary transform
    blend function
    depth test and mask
    alpha test

    multitexture collapsing
    depending on underlying multitexture capability
    we can examine blend funcs and other relevant data
    and collapse into multitexture appropriately
    polygon offset
    used for wall marks, cheezy shadows
  

## Optimized for hardware acceleration ##
    Triangle meshes have a sort key that encodes material state, sort type, etc
    qsort on state before rendering
    1.5M multitexture tris/second on ATI Rage128 on a PIII/500 with 50% of our time in the OpenGL driver

## Triangle renderer ##
    Strip order, but not strips
    32B aligned 1K vertex buffers
    we have knowledge of all rendering data before we begin rendering due to our sorting/batching of primitives
    rescale Z range every frame for max precision in depth buffer
  
    TessEnd_MultitexturedLightmapped
    First texture unit in GL_REPLACE mode
    Second texture unit in GL_MODULATE mode
    Color arrays disabled
    Uses ARB_multitexture extension

    TessEnd_VertexLit
    Handles models
    Same as TessEnd_Generic, just less setup/application cruft on our side, looks the same to the driver

## Scalability ##
    Vertex light option (fill rate bound or lacking blending modes)
    LOD bias for models (throughput bound)
    Subdivision depth for curves (throughput bound)
    Screen re-size (fill bound)
    Dynamic lights can be disabled (CPU bound)
    Supporting multiple CPU architectures

## OpenGL support ##
    Die, minidriver, die
    No support for minidriver or D3D wrapper
    Gave OpenGL an early boost
    Gave OpenGL an anchor to future development
    Wrote Quake III on Intergraph workstations,
    typically dual processor PII/400 machines however with fairly slow (~60mpixel/second) fill rate to keep us honest
    QGL dynamic loading bindings
    LoadLibrary + wglGetProcAddress
    Necessitated because of Voodoo & Voodoo2 ICD vs. minidriver vs. standalone driver
    Allows us to log OpenGL calls

## Transforms ##
    We use the full OpenGL transform pipeline
    We do not use the OpenGL lighting pipeline
    We do not use OpenGL fog capabilities

## Extensions Supported ##
    Written on vanilla OpenGL, extension support is completely optional
    ARB\_multitexture
    texture environment extensions
    S3\_s3tc and other compressed texture extensions
    EXT\_swapinterval
    3DFX\_gamma\_control

## Specific hardware comments ##
  * Note to IHVs: Intel wants to work with you on hardware acceleration,
   contact Igor Sinyak (igor.sinyak@intel.com) if interested
  * Voodoo/V2/V3/Banshee, very good OpenGL driver, 16-bit
  * S3 Savage3D, good OpenGL driver, Savage4 has strong multitexture capability
  * ATI Rage Pro, poor image quality, low performance, but still supported owns the market
  * ATI Rage 128, feature complete, very fast OpenGL Rendition V2200 state of ICD is unknown at this point, but good hardware
  * Intel i740, 16-bit, very high quality OpenGL drivers, AGP texturing, average image quality
  * Matrox G200/G400, looks like they have good hardware and recent strong ICD support
  * Nvidia Riva128(ZX), good OpenGL driver, very poor image quality
  * Nvidia RivaTNT/TNT2, feature complete, very fast OpenGL, Recommendations

## The Future: Content and Technology ##
   * technological advances are second order effects
   * technological advances need appropriate content
   * high resolution art with large dynamic range
   * level design that maximizes the given the triangle budget
   * lighting design that is effective and dramatic
   * game design that leverages the technology effectively

