Brainfuck
=========

Various brainfuck stuff:

1. The BF program "bitwidth.b"; this is something of a torture test, if your interpreter correctly executes this anything else is likely to be dead easy. 

  But the official output of the program is a display of the cell size ie:

  <dl><dt>8 Bit cells<dd>Hello World! 255<dt>16 Bit cells<dd>Hello world! 65535<dt>32 bit cells<dd>Hello, world!</dl>

2. Brainfuck to anything. Well not exactly anything but the list includes ...
  * asmjs -- Convert to the "asm.js" dialect of javascript
  * awk	-- Code for any modern AWK, ancient ones need a #define
  * basic -- A couple of very random BASIC interpreters.
  * bash -- GNU bash, uses arrays, arithimetic etc.
  * bf -- Ook, Blub and fuck fuck
  * cfish -- C and "there once was a fish named Fred"
  * cgmp -- C using the Gnu MP library
  * clojure -- Not a very nice conversion though.
  * dc -- Won't work on dc.sed; the -r option uses a special filter on input.
  * gas -- x64 or x86 assembler. Use gcc to assember and link.
  * lua
  * perl
  * php
  * python
  * ruby
  * ps1 -- That's right MS Powershell
  * neko -- Neko programming language
  * pascal -- Free pascal.
  * run -- a direct interpreter -- blisteringly quick too for one without JIT.
  * tcc -- Convert to C and run using LIBTCC -- Is quicker than bf2run ... just.
  * gcc -- Convert to C and run as a shared lib; very quick with -O2.

  Most are heavily optimised (for Brainfuck) and most work in both 8 bit and the native size of the generated code.
  
