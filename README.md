# OpenArena Engine 
This project is a fork of OpenArena with specific changes to the client and server.

## Building ##

If you are on Ubuntu or Debian, the easiest way to compile this is to install the build dependencies.

```sh
$ sudo apt-get install libcurl4-gnutls-dev libgl1-mesa-dev libsdl2-dev libopus-dev libopusfile-dev libogg-dev zlib1g-dev libvorbis-dev libopenal-dev libjpeg-dev libfreetype6-dev libxmp-dev
$ git clone https://github.com/suijingfeng/engine.git
$ cd engine
$ make
```

You may want to change a few things in Makefile.local.  Other than installing
the build dependencies, you shouldn't need to do anything else.  By default it
builds a modular renderer so you need the binary + *.so or *.dll files in the
same directory to run it.

# OpenArena gamecode 

## Description ##
In mod form it is referred as OpenArenaExpanded (OAX).

## Loading native dll(.so) ##

```
cd linux_scripts/
./supermake_native
```


## Links ##
Development documentation is located here: https://github.com/OpenArena/gamecode/wiki

The development board on the OpenArena forum: http://openarena.ws/board/index.php?board=30.0

In particular the Open Arena Expanded topic: http://openarena.ws/board/index.php?topic=1908.0


## Development ##

```sh
# Get this project or sign up on github and fork it
$ git clone git://github.com/OpenArena/engine.git
$ cd engine

# Create a reference to the upstream project
$ git remote add upstream git://github.com/ioquake/ioq3.git

# View changes in this project compared to ioquake3
$ git fetch upstream
$ git diff upstream/master
```

## Switching renderers ##


This feature is disabled by default.  If you wish to enable it, set USE_RENDERER_DLOPEN=1 in Makefile, 
this allow for build modular renderers and to select the renderer at runtime rather than compiling in one into the binary.
When you start OpenArena, you can pass the name of the dynamic library to load. 

Example:

```sh
# Enable renderergl2:
$ ./openarena.x86_64 +set cl_renderer opengl2

# Enable the default OpenArena renderer:
# This is similiar with the renderergl1 code.
$ ./openarena.x86_64 +set cl_renderer openarena

# Enable the renderergl1:
$ ./openarena.x86_64 +set cl_renderer opengl1

```

## Status ##

* Initial testing on Ubuntu16.04 and Ubuntu18.04

