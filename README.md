Brainfuck
=========

Various brainfuck stuff:

1. The BF program "bitwidth.b"; this is something of a torture test, if your interpreter correctly executes this anything else is likely to be dead easy. 

  But the official output of the program is a display of the cell size ie:

  <dl><dt>8 Bit cells<dd>Hello World! 255<dt>16 Bit cells<dd>Hello world! 65535<dt>32 bit cells<dd>Hello, world!</dl>

2. Brainfuck to anything. Well not exactly anything but the list includes ...
  * asmjs
  * awk
  * basic
  * bash
  * bf -- Ook, Blub and fuck fuck
  * cfish -- C and "there once was a fish named Fred"
  * cgmp -- C using the Gnu MP library
  * clojure
  * dc
  * gas
  * lua
  * neko
  * pascal
  * perl
  * php
  * ps1 -- That's right MS Powershell
  * python
  * ruby
  * run -- a direct interpreter -- blisteringly quick too for one without JIT.
  * tcc -- Convert to C and run using LIBTCC -- Is quicker than bf2run ... just.
  * gcc -- Convert to C and run as a shared lib; very quick with -O2.

  Most are heavily optimised (for Brainfuck) and most work in both 8 bit and the native size of the generated code.
  
