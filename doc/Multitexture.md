# Multitexture

Credit, this article is written by Bryan McNett, 
for original material please visit [here](http://www.bigpanda.com/trinity/article1.html)
, I just take notes here.

## Introduction

This article will attempt to explain one of Quake 3’s most important graphics technologies. 


When cheap 3D hardware acceleration began to proliferate, computer game designers who don’t understand
graphics technology breathed a sigh of relief. "Finally," they said, "all games will look equally good,
and then gameplay will rule the marketplace." In May, 1998, these words bear a whiff of truth: 
games without new graphics technology regularly blow people away simply by supporting 3DFX. 
This will change utterly upon the release of id software’s "Quake 3", the first computer game engine
known to grudgingly accept 3DFX as a minimum requirement.

Light mapping is one Quake technology that changed the computer game industry forever.
Without the detailed shadows made possible by light mapping, it is very difficult to
convey a sense of depth or realism, especially when texture maps repeat. Light mapping
is also an early example of multitexture in computer games. Multitexture refers to the 
act of mixing two or more texture maps to create a new texture map. Multitexture is also
the key to the Quake 3 graphics technology I will discuss. Let’s use light mapping to 
acquaint ourselves with multitexture:

A light map is multiplied with a texture map to produce shadows.
It may not be clear what this means. At these times, it helps to
think of colors as numbers between zero and one, with black having
the value zero and white having the value one. Because white has 
the value one, multiplying it with any other color does not change the color:









## notes

1. proliferate: 增殖, 扩散, 激增; 增殖; 扩散; 激增
2. whiff: 一阵风, 轻轻地吹
3. grudgingly: 勉强地, 不情愿地
