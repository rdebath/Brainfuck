This archive contains the following programs:

 bfc          The compiler for the 'brainfuck' language (240 bytes!)
 bfc.asm      Source for the compiler
 bfi          The interpreter for the 'brainfuck' language
 bfi.c        Source for the interpreter (portable)
 src/         Some example programs in 'brainfuck'
 src/atoi.b   Reads a number from stdin
 src/div10.b  Divides the number under the pointer by 10
 src/hello.b  The ubiquitous "Hello World!"
 src/mul10.b  Multiplies the number under the pointer by 10
 src/prime.b  Computes the primes up the a variable limit
 src/varia.b  Small program elements like SWAP, DUP etc.


WHATS NEW
=========

Yes, I squeezed another ridiculous 56 bytes out of the compiler. They have
their price, though: The new compiler requires OS 2.0, operates on a byte 
array instead of longwords, and generates executables that are always 64K 
in size. Apart from that the language hasn't changed. Again:
***  OS 2.0 *required* for the compiler and the compiled programs  *** 
The interpreter works fine under any OS version. And yes, thanks to Chris
Schneider for his ideas how to make the compiler shorter.


THE LANGUAGE
============

The language 'brainfuck' knows the following commands:

 Cmd  Effect                                 Equivalent in C
 ---  ------                                 ---------------
 +    Increases element under pointer        array[p]++;
 -    Decrases element under pointer         array[p]--;
 >    Increases pointer                      p++;
 <    Decreases pointer                      p--;
 [    Starts loop, counter under pointer     while(array[p]) {
 ]    Indicates end of loop                  }
 .    Outputs ASCII code under pointer       putchar(array[p]);
 ,    Reads char and stores ASCII under ptr  array[p]=getchar();

All other characters are ignored. The 30000 array elements and p are being
initialized to zero at the beginning.  Now while this seems to be a pretty
useless language, it can be proven that it can compute every solvable
mathematical problem (if we ignore the array size limit and the executable
size limit).


THE COMPILER
============

The compiler does not check for balanced brackets; they better be. It reads
the source from stdin and writes the executable to stdout. Note that the 
executable is always 65536 bytes long, and usually won't be executable if
brackets aren't balanced. OS 2.0 required for the compiler and the compiled
program.


THE INTERPRETER
===============

For debugging, there's also an interpreter. It expects the name of the 
program to  interpret on the command line and accepts an additional command:
Whenever a '#' is met in the source and a second argument was passwd to
the interpreter, the first few elements of the array are written to stdout
as numbers


   Enjoy

     -Urban Mueller     <umueller@amiga.physik.unizh.ch>
