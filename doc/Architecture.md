# Quake III Architecture Overview

![](https://github.com/suijingfeng/engine/blob/master/doc/q3_workspace_architecture2.png)


## Introduction

Note: this is an ASCII dump of the http://fabiensanglard.net/quake3/index.php and 
http://users.suse.com/%7Elnussel/talks/fosdem_talk_2013_q3.pdf. 


Quake III engine is mostly an evolution of idTech2 but there are some interesting novelties. The key points can be summarized as follow: 


* New dualcore renderer with material based shaders.
* New Network model based on snapshots.
* New Virtual Machines playing an essential part in the engine, 
combining Quake1 portability/security with Quake2 speed.
* New Artificial Intelligence for the bots.


 
Two important things to understand the design:

1. Every single input (keyboard, win32 message, mouse, UDP socket) is converted into
an event_t and placed in a centralized event queue (sysEvent_t eventQue[256]). This
allows among other things to record (journalize) each inputs in order to recreate
bugs. This design decision was discussed at length in John Carmack's .plan on 
Oct 14, 1998.

2. Explicit split of Client and Server. The explicit split of networking into a
client presentation side and the server logical side was really the right thing
to do. We backed away from that in Doom 3 and through most of Rage, but we are 
migrating back towards it. All of the Tech3 licensees were forced to do somewhat
more work to achieve single player effects with the split architecture, but it 
turns out that the enforced discipline really did have a worthwhile payoff.

3. The server side is responsible for maintaining the state of the game, 
determine what is needed by clients and propagate it over the network. 
It is statically linked against bot.lib which is a separate project


## Memory allocation

Two custom allocators at work here:

* Zone Allocator: Responsible for runtime,small and short-term memory allocations.
* Hunk Allocator: Responsible for on level load, big and long-term allocations from the pak files (geometry,map, textures, animations).



## the Game Modules

### Game
* run on server side
* maintain game state
* compute bots
* move objects

### CGame
* run on client side
* predict player states
* tell sound and gfx renderers what to show

### UI
* run on client side
* show menus



## The Quake III Way for Extensions a Program

### Bytecode
* compiled by special compiler (lcc)
* byte code interpreted by a virtual machine
* strict sandbox, strict memory limit
* main program can catch exceptions and unload VM

### Native
* writen in C and compiler as shared library
* must restrict to embedded libc subset
* must not use external libs(such as malloc)


## The Virtual Machine Architecture
* 32bit addressing, little endian
* 32bit floats
* about 60 instructions
* separate memory addressing for code and data
* no dynamic memory allocations
* no GP registers, load/store on opstack

## opStack vs Program Stack

### Program stack
* local variables
* function parameters
* saved program counter

### opStack

* arguments for instructions, results
```
OP CONST 3
OP CONST 4
OP ADD
```
* function return values

## Calling Convention
    parameters and program counter pushed to stack
    callee responsible for new stack frame
    negative address means syscall
    return value on opstack

## VM Memory Layout
![](https://github.com/suijingfeng/engine/blob/master/doc/VM_Memory_Layout.png)

    vm memory allocated as one block
    64k stack at end of image
    code and opstack separate
    memory size rounded to power of two
    simple access violation checks via mask
    cannot address memory outside that block
    can¡¯t pass pointers

## Calling into the VM
```
intptr_t vmMain ( int command, int arg0, int arg1, ..., int arg11 )
{
    switch( command )
    {
        case GAME INIT:
            G_InitGame(arg0, arg1, arg2);
            return 0;
        case GAME SHUTDOWN:
            G_ShutdownGame(arg0);
            return 0;
        ...
    }
}    
```
    code is one big block, no symbols
    can only pass int parameters
    start of code has to be dispatcher function
        first parameter is a command 
        called vmMain() for dlopen()

## Calling into the Engine

* requests into engine
    print something
    open a file
    play s sound
* expensive operations
    memset, memcpy, sinus, cosinus, vector operation
* pass pointers but not return!

## More Speed With Native Code
* Byte code interpreter too slow for complex mods
* Translate byte code to native instructions on load
* CPU/OS specific compilers needed
    currently: x86, x86_64, ppc, sparc
* Multiple passes to calculate offsets
* Needs extra memory as native code is bigger
* Need to be careful when optimizing jump targets
