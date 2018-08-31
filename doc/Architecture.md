# Quake III Architecture Overview

![](https://github.com/suijingfeng/engine/blob/master/doc/q3_workspace_architecture2.png)


## Introduction

Note: this is an ASCII dump of the http://fabiensanglard.net/quake3/index.php and 
http://users.suse.com/%7Elnussel/talks/fosdem_talk_2013_q3.pdf. 


Quake III engine is mostly an evolution of idTech2 but there are some interesting novelties. The key points can be summarized as follow: 


* New dualcore renderer with material based shaders.
* New Network model based on snapshots.
* New Virtual Machines playing an essential part in the engine, 
combining Quake1 portability/security with Quake2 speed. This is achieved by compiling the bytecode to x86 instruction on the fly.
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

If previous engines delegated only the gameplay to the Virtual Machine,
idtech3 heavily rely on them for essential tasks. Among other things: 

* 32bit addressing, little endian
* 32bit floats
* about 60 instructions
* separate memory addressing for code and data
* no dynamic memory allocations
* no GP registers, load/store on opstack
* Rendition is triggered from the Client VM.
* The lag compensation mechanism is entirely in the Client VM.

Moreover their design is much more elaborated: 
They combine the security and portability of Quake1 Virtual Machine 
with the high performances of Quake2's native DLLs.
This is achieved by compiling the bytecode to x86 instruction on the fly.


The virtual machine was initially supposed to be a plain bytecode interpreter 
but performances were disappointing so the development team wrote a runtime x86 compiler.
According to the .plan from Aug 16, 1999.


As I mentioned at quakecon, I decided to go ahead and try a dynamic
code generator to speed up the game interpreters. I was uneasy about it,
but the current performance was far enough off of my targets that I didn¡¯t
see any other way. 

At first, I was surprised at how quickly it was going. 
The first day, I worked out my system calling conventions and execution environment and
implemented enough opcode translations to get ¡±hello world¡± executing.

The second day I just plowed through opcode translations, tediously generating a lot of code like this:


```
case OP_NEGI:
    EmitString( "F7 1F" );      // neg dword ptr [edi]
    break;
case OP_ADD:
    EmitString( "8B 07" );     // mov eax, dword ptr [edi]
    EmitString( "01 47 FC" );  // add dword ptr [edi-4],eax
    EmitString( "83 EF 04" );  // sub edi,4
    break;
case OP_SUB:
    EmitString( "8B 07" );     // mov eax, dword ptr [edi]
    EmitString( "29 47 FC" );  // sub dword ptr [edi-4],eax
    EmitString( "83 EF 04" );  // sub edi,4
    break;
case OP_DIVI:
    EmitString( "8B 47 FC" );  // mov eax,dword ptr [edi-4]
    EmitString( "99" );        // cdq
    EmitString( "F7 3F" );     // idiv dword ptr [edi]
    EmitString( "89 47 FC" );  // mov dword ptr [edi-4],eax
    EmitString( "83 EF 04" );  // sub edi,4
    break;
```

In Quake III a virtual machine is called a QVM: Three of them are loaded at any time:

* Client Side: Two virtual machine are loaded. Message are sent to one or the other depending on the gamestate:
        cgame : Receive messages during Battle phases. Performs entity culling, predictions and trigger renderer.lib.
        q3_ui : Receive messages during Menu phases. Uses system calls to draw the menus.

* Server Side:
        game : Always receive message: Perform gamelogic and hit bot.lib to perform A.I .


### QVM Internals

Before describing how the QVMs are used, let's check how the bytecode is generated.
As usual I prefer drawing with a little bit of complementary text: 

![](https://github.com/suijingfeng/engine/blob/master/doc/vm_chain_prod.png)

quake3.exe and its bytecode interpreter are generated via Visual Studio but the VM bytecode takes a very different path: 


1. Each .c file (translation unit) is compiled individually via LCC.

2. LCC is used with a special parameter so it does not output a PE (Windows Portable Executable) 
but rather its Intermediate Representation which is text based stack machine assembly. 
Each file produced features a text, data and bss section with symbols exports and imports.

3. A special tool from id Software q3asm.exe takes all text assembly files and assembles them together in one .qvm file. 
It also transform everything from text to binary (for speed in case the native converted cannot kick in). 
q3asm.exe also recognize which methods are system calls and give those a negative symbol number. 

4. Upon loading the binary bytecode, quake3.exe converts it to x86 instructions (not mandatory).


### LCC Internals

Here is a concrete example starting with a function that we want to run in the Virtual Machine: 

```
    extern int variableA;
    int variableB;
    int variableC=0;
    
    int fooFunction(char* string)
    {    
        return variableA + strlen(string);   
    }
```

Saved in module.c translation unit, lcc.exe is called with a special flag in order to 
avoid generating a Windows PE object but rather output the Intermediate Representation.
This is the LCC .obj output matching the C function above:

```
    data
    export variableC
    align 4
    LABELV variableC
    byte 4 0
    export fooFunction
    
    code
    proc fooFunction 4 4
    ADDRFP4 0
    INDIRP4
    ARGP4
    ADDRLP4 0
    ADDRGP4 strlen
    CALLI4
    ASGNI4
    ARGP4 variableA
    INDIRI4
    ADDRLP4 0
    INDIRI4
    ADDI4
    RETI4
    LABELV $1
    endproc fooFunction 4 4
    import strlen
    
    bss
    export variableB
    align 4
    LABELV variableB
    skip 4
    import variableA

```

A few observations:

* The bytecode is organized in sections (marked in red): 
    We can clearly see the bss (uninitialized variables), 
    data (initialized variables) and 
    code (usually called text but whatever..)
* Functions are defined via proc, endproc sandwich (marked in blue).
* The Intermediate Representation of LCC is a stack machine: 
    All operations are done on the stack with no assumptions made about CPU registers.
* At the end of the LCC phrase we have a bunch of files importing/exporting variables/functions.
* Each statement starts with the operation type (i.e: ARGP4, ADDRGP4, CALLI4...). Every parameter and result will be passed on the stack.
* Import and Export are here so the assembler can "link" translation units" together. 
    Notice import strlen, since neither q3asm.exe nor the VM Interpreter actually likes to the C Standard library, 
    strlen is considered a system call and must be provided by the Virtual Machine.

Such a text file is generated for each .c in the VM module. 


## q3asm.exe Internals
q3asm.exe takes the LCC Intermediate representation text files and assembles them together in a .qvm file:
![](https://github.com/suijingfeng/engine/blob/master/doc/vm_q3asm.png)


Several things to notice:

* q3asm makes sense of each import/export symbols across text files.
* Some methods are predefined via a system call text file. 
    You can see the syscall for the client VM and for the Server VMs.
    System calls symbols are attributed a negative integer value so they can be identified by the interpreter.
* q3asm change representation from text to binary in order to gain space and speed but that is pretty much it, no optimizations are performed here.
* The first method to be assembled MUST be vmMain since it is the input message dispatcher. 
    Moreover it MUST be located at 0x2D in the text segment of the bytecode.

## QVM: How it works

Again a drawing first illustrating the unique entry point and unique exit point that act as dispatch:
![](https://github.com/suijingfeng/engine/blob/master/doc/vm_bb.png)

A few details:

Messages (Quake3 -> VM) are send to the Virtual Machine as follow:

* Any part of Quake3 can call VM_Call( vm_t *vm, int callnum, ... ).
* VMCall takes up to 11 parameters and write each 4 bytes value in the VM bytecode (vm_t *vm) from 0x00 up to 0x26.
* VMCall writes the message id at 0x2A.
* The interpreter starts interpreting opcodes at 0x2D (where vmMain was placed by q3asm.exe).
* vmMain act as a dispatch and route the message to the appropriate bytecode method.

You can find the list of Message that can be sent to the [Client VM](https://github.com/id-Software/Quake-III-Arena/blob/master/code/cgame/cg_public.h)
and [Server VM](https://github.com/id-Software/Quake-III-Arena/blob/master/code/game/g_public.h) (at the bottom of each file).

System calls (VM -> Quake3) go out this way:

1. The interpreter execute the VM opcodes one after an other (VM_CallInterpreted).
2. When it encounters a CALLI4 opcode it checks the int method index.
3. If the value is negative then it is a system call.
4. The "system call function pointer" (int (*systemCall)( int *parms )) is called with the parameters.
5. The function pointed to by systemCall acts as a dispatch and route the system call to the right part of quake3.exe

* Parameters are always very simple types: Either primitives types (char,int,float) or pointer to primitive types `(char* , int[])`. 
I suspect this was done to minimize issues due to struct alignment between Visual Studio and LCC.

* Quake3 VM does not perform dynamic linking so a developer of a QVM mod had no access to any library, 
not even the C Standard Library (strlen, memset functions are here...but they are actually system calls). 
Some people still managed to fake it with preallocated buffer: [Malloc in QVM](http://icculus.org/homepages/phaethon/q3/malloc/malloc.html) !! 


## Productivity issue and solution

With such a long toolchain, developing VM code was difficult:

    The toolchain was slow.
    The toolchain was not integrated to Visual Studio.
    Building a QVM involved using commandline tools. It was cumbersome and interrupted the workflow.
    With so many elements in the toolchain it was hard to identify which part was at fault in case of bugs.

So idTech3 also have the ability to load a native DLL for the VM parts and it solved everything:
![](https://github.com/suijingfeng/engine/blob/master/doc/vm_chain_dev.png)

Overall the VM system is very versatile since a Virtual Machine is capable of running:

* Interpreted bytecode
* Bytecode compiled to x86 instructions
* Code compiled as a Windows DLL


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
    can't pass pointers

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
