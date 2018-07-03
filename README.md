# OpenArena Engine 
This project is a fork of OpenArena with specific changes to the client and server.

## Building on Ubuntu or Debian Linux##


Install the build dependencies.

```sh
$ sudo apt-get install libcurl4-gnutls-dev libgl1-mesa-dev libsdl2-dev libopus-dev libopusfile-dev libogg-dev zlib1g-dev libvorbis-dev libopenal-dev libjpeg-dev libfreetype6-dev libxmp-dev
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
Second, extract the data files on ~/.OpenArena/ (linux) or C:\Users\youname\AppData\Roaming\OpenArena\ (windows)


```sh
$ cd /build/release-linux-x86_64/
$ ./openarena.x86_64
```

By default it load renderer\_openarena\_x86\_64.so (if build in linux).

## Switching renderers ##


This feature is enabled by default. If you wish to disable it, set USE\_RENDERER\_DLOPEN=0 in Makefile, 
this allow for build modular renderers and to select the renderer at runtime rather than compiling in one into the binary.
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
use gprof to examine the 
```
gprof openarena.x86_64 gmon.out > report.txt
```
