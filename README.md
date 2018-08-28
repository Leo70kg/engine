# OpenArena Engine 
This project is a fork of OpenArena with specific changes to the client and server.
Some of the major features currently implemented are:

  * SDL 2 backend
  * OpenAL sound API support (multiple speaker support and better sound
    quality)
  * Full x86_64 support on Linux
  * VoIP support, both in-game and external support through Mumble.
  * MinGW compilation support on Windows and cross compilation support on Linux
  * AVI video capture of demos
  * Much improved console autocompletion
  * Persistent console history
  * Colorized terminal output
  * Optional Ogg Vorbis support
  * Much improved QVM tools
  * Support for various esoteric operating systems
  * cl_guid support
  * HTTP/FTP download redirection (using cURL)
  * Multiuser support on Windows systems (user specific game data
    is stored in "%APPDATA%\OpenArena")
  * PNG support
  * Many, many bug fixes

The original id software readme that accompanied the Q3 source release has been
renamed to id-readme.txt so as to prevent confusion. Please refer to the
website for updated status.



## Building on Ubuntu or Debian Linux ##


Install the build dependencies.

```sh
$ sudo apt-get install libcurl4-openssl-dev libsdl2-dev libopenal-dev
$ sudo apt-get install libgl1-mesa-dev libopus-dev libopusfile-dev libogg-dev zlib1g-dev libvorbis-dev libjpeg-dev libfreetype6-dev libxmp-dev
$ sudo apt-get install clang
$ git clone https://github.com/suijingfeng/engine.git
$ cd engine
$ make
```

## Building on Windows 7 or 10 ##

To build 64-bit binaries, follow these instructions:

1. Install msys2 from https://msys2.github.io/ , following the instructions there.
2. Start "MinGW 64-bit" from the Start Menu, NOTE: NOT MSYS2.
3. Install mingw-w64-x86\_64, make, git tools:
```sh
pacman -S mingw-w64-x86_64-gcc make git
```
4. Grab latest openarena source code from github and compile. Note that in msys2, your drives are linked as folders in the root directory: C:\ is /c/, D:\ is /d/, and so on.

```sh
git clone https://github.com/suijingfeng/engine.git
cd engine
make ARCH=x86_64
```
5. Find the executables and dlls in build/release-mingw64-x86\_64 . 



## RUN ##
First, download the map packages from http://openarena.ws/download.php
Second, extract the data files at
~/.OpenArena/ (on linux) 
C:\Users\youname\AppData\Roaming\OpenArena\ (on windows)


```sh
$ cd /build/release-linux-x86_64/
$ ./openarena.x86_64
```


## Switching renderers ##


This feature is disabled by default. If you wish to enable it, set USE\_RENDERER\_DLOPEN=1 in Makefile.
This allow for build modular renderers and select the renderer at runtime rather than compiling into one binary.
When you start OpenArena, you can pass the name of the dynamic library to load. 

Example:

```sh
# Enable renderergl2:
$ ./openarena.x86_64 +set cl_renderer opengl2

# Enable the renderergl1:
$ ./openarena.x86_64 +set cl_renderer opengl1

# Enable the default OpenArena renderer:
# This renderer module is similiar to the renderergl1 code.
$ ./openarena.x86_64 +set cl_renderer openarena
```


# OpenArena gamecode

## Description ##
It's non engine part of OA, includes game, cgame and ui.
In mod form it is referred as OAX. 

## Loading native dll(.so) ##

```
cd linux_scripts/
./supermake_native
```


## Links ##
Development documentation is located here: https://github.com/OpenArena/gamecode/wiki

The development board on the OpenArena forum: http://openarena.ws/board/index.php?board=30.0

In particular the Open Arena Expanded topic: http://openarena.ws/board/index.php?topic=1908.0



## Status ##

* Initial testing on Ubuntu16.04 and Ubuntu18.04, Win7 and Win10


## License ##

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

## extra ##

* About com\_hunkmegs

When i playing CTF on :F for stupid server with default com\_hunkmegs = 128 setting, the following errors occurs:
`
ERROR: Hunk\_Alloc failed on 739360: code/renderergl2/tr\_model.c, line: 535 (sizeof(*v) * (md3Surf->numVerts * md3Surf->numFrames)).
`
OpenGL2 renderer seems use more memory, Upping com\_hunkmegs to 256 will generally be OK.


* About -fno-strict-aliasing
I am using GCC7.2 and clang6.0 on ubuntu18.04.

Build OA using clang with -fno-strict-aliasing removed:
`
WARNING: light grid mismatch, l->filelen=103896, numGridPoints*8=95904
`
This is printed by renderergl2's R_LoadLightGrid function.

    Problem solved with following line added in it.
`
ri.Printf( PRINT_WARNING, "s_worldData.lightGridBounds[i]=%d\n", s_worldData.lightGridBounds[i]);
` 
    However this line do nothing but just printed the value of s_worldData.lightGridBounds[i]. I guess its a bug of clang.

    Build OA with GCC without this issue.


* Use gprof to examine the performance of the program
```
gprof openarena.x86_64 gmon.out > report.txt
```
