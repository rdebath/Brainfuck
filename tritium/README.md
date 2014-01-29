Tritium
=======

This is the fastest brainfuck interpreter around, it's official name is Ρ‴ but as most people won't pronounce that right it's working name is Tritium.

It's optimisations include:

 * Run length encoding.
 * Change '[-]' and '[+]' to value set instructions.
 * Change "Move or ADD" ( MADD ) loops to value set instructions.
 * Change more complex loops it the initial states are known
 * Preserve known values through loops.
 * Downgrade loops to conditionals.
 * Eliminate redundant writes.
 * Eliminate redundant reads.
 * Convert known Print instructions into constant prints and eliminate the related tape setting.

It has multiple backends.
 * JIT compile using GUN Lightning.
 * Two JIT C compiles, one using libtcc the other using gcc and dlopen().
 * A 'Tree' interpreter for debugging, profiling and tracing.
 * An array interpreter as a reasonably fast fallback for the JIT.
 * Generate C code or later compiling.
 * Generate NASM assembler to directly create an ELF32 executable.
 * Generate a program for the unix dc(1) command.

And finally there the 'cheat' option '-Orun', this runs the syntax tree as far as it can with the 'Tree' interpreter before running a code generator. If the BF program has no input (',') commands this will run the program to completetion and convert it into a 'Hello World'.
