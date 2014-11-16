Tritium
=======

This is the fastest brainfuck interpreter around, it's official name is Ρ‴ but as most people won't pronounce that right it's working name is Tritium.

It's optimisations include:

 * Run length encoding.
 * Change '[-]' and '[+]' to value set instructions.
 * Change "Move or ADD" ( MADD ) loops to value set instructions.
 * Change more complex loops if the initial states are known
 * Preserve known values through loops.
 * Downgrade loops to conditionals.
 * Eliminate redundant writes.
 * Eliminate redundant reads.
 * Convert known Print instructions into constant prints and eliminate the related tape write.

It has multiple backends.
 * JIT compile using GUN Lightning.
 * JIT compile using luajit's dynasm.
 * Two JIT C compiles, one using libtcc the other using gcc and dlopen().
 * A 'Tree' interpreter for debugging, profiling and tracing.
 * An array interpreter as a reasonably fast fallback for the JIT.
 * Generate C code for later compiling.
 * Generate macro calls for translation to Ruby, Perl, PHP, Awk etc.
 * Generate NASM assembler to directly create an ELF32 executable (for example a 116 byte Hello World).
 * Generate NASM or gas (intel) assembler to link with libc.
 * Generate a program for the unix dc(1) command.

And finally there is the 'cheat' option '-Orun', this runs the syntax tree as far as it can with the 'Tree' interpreter before running a code generator. If the BF program has no input (',') commands this will run the program to completion (or hang) and convert it into a 'Hello World'. However, as this currently is done using the tree interpreter it runs at less than a tenth the speed of the JIT runners.

Though even without "-Orun" it'll convert a standard 'Hello world!' and several other 'benchmark' programs into C code like this:
<pre>
#include &lt;stdio.h&gt;

int main(void)
{
  puts("Hello World!");
  return 0;
}
</pre>
