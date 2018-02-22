Brainfuck
=========

This repository contains various [brainfuck](http://esolangs.org/wiki/Brainfuck) stuff.

The classic description of BF is the C translation below, the opcode column is the direct translation used by my Tritium interpreter.

| Opcode | Brainfuck   | C                   |
| -------|:-----------:|---------------------|
| T_MOV  | `>`         | ++ptr;              |
| T_MOV  | `<`         | --ptr;              |
| T_ADD  | `+`         | ++*ptr;             |
| T_ADD  | `-`         | --*ptr;             |
| T_WHL  | `[`         | while (*ptr) {      |
| T_END  | `]`         | }                   |
| T_PRT  | `.`         | putchar(*ptr);      |
| T_INP  | `,`         | ptr[0] = getchar(); |

There are several BF interpreters and tools in this repository.

1. The BF program "bitwidth.b"; this is something of a torture test, if your interpreter correctly executes this anything else is likely to be dead easy.

    But the official output of the Bitwidth program is a display of the cell size ie:

    <dl><dt>8 Bit cells<dd>Hello World! 255<dt>16 Bit cells<dd>Hello world! 65535<dt>32 bit cells<dd>Hello, world!<dt>8 Bit cells without wrapping<dd>Hello World! Ltd</dl>

    Though for a quick check this "Hello World!" program may be better, however, it has far fewer checks.

    ```bf
    ++++[>++++<-]>[>+>++>[+++++++>]+++[<]>-]>>>>>>>>-.
    <<<<.<..+++.<.>>>>.<<<.+++.------.>-.<<+.<------.
    ```

2. VIM Syntax highlighting file for brainfuck.

    This file highlights the eight BF command characters in four (reasonable) classes. This is the simple part.

    In addition it highlights SOME of the other characters as comments.  Generally it tries (with moderate success) to distinguish between proper comments and sequences that are probably supposed to be comments but actually contain active BF command characters. In addition it tries to identify the 'dead code' style comment loops highlighting any BF command characters within the loop in the 'PreProc' style to distinguish them from commands that may actually be executed.

3. Tritium (officially Ρ‴) this BF interpreter/compiler/JIT runner makes other programs look slow. It is simply the fastest BF interpreter you'll find. For top speed the cell size should be 8 or 32bits, but it supports ANY fixed bit cell size.

4. Brainfuck to anything. Well not *exactly* anything but the list includes ...
    * run -- a direct interpreter -- blisteringly quick too for one without JIT.
    * jit -- OTOH this one uses LuaJIT's Dynasm, it's the fastest bf2any program.
    * crun -- Convert to C and run using libtcc or libdl. Using TCC it's quicker than bf2run ... just, GCC is a lot quicker, even without GCC doing any optimisation.
    * bf -- Ook, Blub, fuck fuck, "there once was a fish named Fred" and over 50 similar transliterations. Also includes Cell doubler (and quad) mappings. Some can be compiled as C (most are *deoptimised* but -rle is not.)
    * asmjs -- Convert to the "asm.js" dialect of javascript (Includes a nodejs wrapper)
    * awk	-- Code for (almost) any version of AWK.
    * basic -- A few very random BASIC interpreters.
    * bn -- C using the OpenSSL bignum library.
    * cgmp -- C using the Gnu MP library. This uses a linked list so the pointer movements are slower.
    * clojure -- Not a very nice conversion though. *no-opt*
    * cmd -- Windows batch files ... far too slow for testing
    * cobol -- The old language using the "open-cobol" variation.
    * cs -- Microsoft's C# language.
    * csh -- The old C-shell, this is really slow.
    * d -- The C replacement originally by 'Digital Mars'
    * dc -- The unix command, has a -r (run) option that uses a special filter to allow character input. This is too complex for dc.sed.
    * dump -- Dump the tokens sent to the BE, plus C.
    * elf -- Direct production of a 32bit Linux executable. *no-opt*
    * f90 -- Fortran, the 1990 variant.
    * gas -- x64 or x86 assembler. Use gcc to assemble and link: "gcc -o bfp bfout.s" *no-opt*
    * go -- Google's modern language
    * java --
    * julia -- [A modern language using LLVM](http://julialang.org)
    * ksh -- Two variants one for ksh88 and one for the current version.
    * lua --
    * navision -- Now called Microsoft DynamicsNAV
    * neko -- [Neko programming language VM](http://nekovm.org)
    * oldsh -- Doesn't use POSIX arithmetic nor the expr command.
    * pascal -- Free pascal.
    * perl --
    * php --
    * ps1 -- That's right MS Powershell
    * python -- Python2 or Python3
    * rc -- The Plan9 shell rc(1) (Can't input without external programs.) -- *deoptimised*
    * ruby --
    * s-lang --
    * swift -- Apple's script language
    * sh -- Bourne shell with posix extensions: ash, bash, dash, ksh, lksh, mksh, pdksh-5.2.12+, yash, zsh.
    * t-sql -- Microsoft's SQL based language.
    * tcl --
    * whitespace -- A good translation to the Forth-like whitespace eso-lang. (and "semicolon")

    Most are heavily optimised (for Brainfuck) and most work in both 8 bit and the native size of the generated code.
    The ones marked *no-opt* are not properly optimised as the BE can't generate all the needed op-codes. The ones marked *deoptimised* have even the RLE reverted. The FE can still optimise in all cases, but without the BE optimisation only constant folding will occur.
    They have all been tested using many of the BF programs from the [Esoteric Files Archive](https://github.com/graue/esofiles/tree/master/brainfuck/src) (And of course tortured!)

5. The 'extras' subdirectory.
    * bf.pl -- A very small bf->perl translate and execute.
    * bf.rb -- A similar program in Ruby, not as Golfed.
    * bf.sed -- A BF interpreter in SED.
    * bf2bash.sh -- A BF interpreter in bash
    * bf2c-plain.sed -- A plain BF->C convert in sed.
    * bf2c.awk -- A reasonably quick BF->C and run in awk.
    * bf2c.rb -- A very compact BF->C and run in Ruby.
    * bf2c.sed -- A slightly optimising BF->C in sed.
    * bf2c_v1.b -- A Plain BF->C in BF.
    * bf2c_v2.b -- A weird RLE optimising BF->C in BF.
    * bf2lua.lua -- A RLE optimising BF->lua and run in lua.
    * bfdowhile.c -- A "buggy" BF interpreter that is (surprisingly) still Turing complete.
    * bfopt.c -- A Brainfuck to brainfuck optimiser.
    * blub.pl -- "Blub" interpreter in Perl
    * byte2byte.c -- Optimal two cell text to BF converter.
    * cdowhile.c -- A "buggy" BF interpreter that is (surprisingly) still Turing complete.
    * dblmicrobf.c -- A plain BF interpreter with weird cell sizes.
    * deadbeef.c -- A nice quick interpreter that ONLY has a two command lookahead.
    * easy.c -- A pipelined BF interpreter (or an interpreter for the 'easy' language).
    * Hello_world.bewbs -- An interpreter for strange BF variant.
    * hydrogen.c -- A component of Tritium that also understands some cell doubling constructs.
    * k-on-fuck.pl -- Another BF variant in perl.
    * lightning.c -- A plain jit interpreter using GNU lightning V2.
    * malbrain.sed -- Convert 'malbrain' to C in sed.
    * md5.gw.b -- These two BF programs have the same MD5 hash, but produce different outputs.
    * md5.hw.b -- Ditto.
    * microbf.c -- A compact and slow BF interpreter.
    * neutron.c -- The input and tree/stack overlap prototype for Tritium's input routine.
    * ook.l -- A program in Lex that interprets about 20 different trivial BF substitutions (including Ook).
    * ook.pl -- A perl program to interpret Ook.
    * petooh.rb -- A Ruby problem to interpret 'Petooh'
    * pikalang.rb -- A 'Pikalang' interpreter in Ruby.
    * pogaack.pl -- A Perl program to interpret 'Pogaack'
    * profilebf.c -- A BF interpreter for counting and measuring Brainfuck.
    * proton.c -- Another component interpreter for Tritium.
    * spoon.rb -- A 'Spoon' interpreter in Ruby.
    * trixy.c -- Conversion of Brainfuck to EXCON, ABCD, Binerdy and Tick.
    * txtbf.b -- A BF program to generate BF programs that output specific text strings.
    * txtbf.c -- A program to generate BF programs that output specific text strings.
    * zero.rb -- A Ruby program to interpret 'Zerolang'.

6. The 'umueller' directory contains the second brainfuck compiler written by Urban Müller.
  It was written in June 1993.  The previous one was 296 bytes long, used longword cells (That's 32bits on the Amiga) and did not require 'Amiga OS 2.0'.

Copyrights
==========

The bf.vim, bf2any and Tritium programs are licensed under the GPLv2 or later versions if you prefer.

The programs bf2bash.sh, bf2c.awk and all brainfuck source code, including any
embedded in the C code are explicitly granted public domain status.

The code in the tools and the testing directories have their own licensing.
