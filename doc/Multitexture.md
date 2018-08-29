# Multitexture

Credit, this article is written by Bryan McNett, 
for original material please visit [here](http://www.bigpanda.com/trinity/article1.html)
, I just take notes here.

## Introduction

This article will attempt to explain one of Quake 3's most important graphics
technologies. 

When cheap 3D hardware acceleration began to proliferate, computer game designers
who don't understand graphics technology breathed a sigh of relief. "Finally," 
they said, "all games will look equally good, and then gameplay will rule the
marketplace." In May, 1998, these words bear a whiff of truth: games without new
graphics technology regularly blow people away simply by supporting 3DFX. This will
change utterly upon the release of id software's "Quake 3", the first computer game
engine known to grudgingly accept 3DFX as a minimum requirement.

Light mapping is one Quake technology that changed the computer game industry
forever. Without the detailed shadows made possible by light mapping, it is very
difficult to convey a sense of depth or realism, especially when texture maps
repeat. Light mapping is also an early example of multitexture in computer games.
Multitexture refers to the act of mixing two or more texture maps to create a new
texture map. Multitexture is also the key to the Quake 3 graphics technology I will
discuss. Let£ªs use light mapping to acquaint ourselves with multitexture:

A light map is multiplied with a texture map to produce shadows. It may not be clear
what this means. At these times, it helps to think of colors as numbers between zero
and one, with black having the value zero and white having the value one. Because
white has the value one, multiplying it with any other color does not change the
color. Because black has the value zero, multiplying it with any other color changes
the color to black. In a light map, shadows are black and the rest is bright. 
Therefore, when the light map is multiplied with the texture map, shadows become
black and the rest stays essentially the same:

![smaller](https://github.com/suijingfeng/engine/blob/master/doc/whateverXone.png)


Let's look at what happens to one pixel when a light map is multiplied with a
texture map. In this example, I will use pseudocode because it is familiar to
programmers. P stands for "pixel color", T for "texture color", and L for 
"light map color":
```
P.r = L.r * T.r; // red
P.g = L.g * T.g; // green
P.b = L.b * T.b; // blue
```
To find the pixel color, we multiply the values of the texture map and light map.
Since we do this once for each pixel, we may say that we are multiplying the texture
maps themselves.

At this point, we should abandon pseudocode in favor of something with the same
meaning, but a more compact form: 



![formular](https://github.com/suijingfeng/engine/blob/master/doc/Image3.gif)
