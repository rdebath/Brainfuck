
This is a collection of other BF interpreters, compilers, tools and components.

The files bf2c_v1.b and bf2c_v2.b are simple BF to C converters the second does RLE (strangely). The other bf2c* are also converters from BF to C written in their respective languages.

The C programs are all interpreters, mostly components that were merged into one of my major interpreters or attempts on the theme of "small but tidy".

The perl (*.pl) ones are very simple converters for various BF equivalent languages; all based on the original bf.pl.

The files bf2bash.sh, bf2lua.lua and also bf.pl are interpreters that convert to the host language then use an "eval" command.

The txtbf.c program takes a text input and generates a brainfuck program to output that text, in default mode it's quick and effective. In "-max" mode it's very slow and thorough.

Here's the full list.

File | Description
-----|------------
bf2bash.sh | Convert Brainfuck to bash and "eval" it
bf2c.awk | Convert Brainfuck to C, then compile and run *optimises*
bf2c-plain.sed | Very plain brainfuck to C in sed
bf2c.sed | Lightly optimising brainfuck to C
bf2c_v1.b | Unoptimised brainfuck to C in brainfuck
bf2c_v2.b | Slightly optimising brainfuck to C in brainfuck
bf2lua.lua | Lightly optimising brainfuck to Lua in Lua, including Lostkng.b
bfdowhile.c | Brainfuck variant that seems (surprisingly) to be Turing complete
bf.pl | Golfed brainfuck to Perl and eval
bf.rb | Golfed brainfuck to Ruby and eval
bf.sed | Brainfuck interpreter in SED, no input.
blub.pl | [Blub](http://esolangs.org/wiki/Blub) interpreter.
cdowhile.c | Turing complete brainfuck variant to C and run.
dblmicrobf.c | microbf variants
deadbeef.c | Brainfuck interpreter with TWO instruction lookahead, rather quick.
easy.c | [Easy](http://esolangs.org/wiki/Easy) interpreter and 16bit brainfuck.
Hello_world.bewbs | Another brainfuck variant (Perl eval)
hydrogen.c | Early component for tritium.
k-on-fuck.pl | Another brainfuck variant (Perl eval)
malbrain.sed | [Malbrain](http://esolangs.org/wiki/Malbrain) conversion to C in sed.
microbf.c | Nice tiny brainfuck interpreter
neutron.c | Testbed interpreter for brainfuck smart parser.
ook.pl | [Ook](http://esolangs.org/wiki/Ook) interpreter.
petooh.rb | [Petooh](https://github.com/Ky6uk/PETOOH) "eval" interpreter (brainfuck variant)
pogaack.pl | [POHAACK](http://esolangs.org/wiki/POGAACK) interpreter (brainfuck variant with RLE)
profilebf.c | A counting brainfuck interpreter, counts and classifies your brainfuck code.
proton.c | A simple testbed brainfuck interpreter.
txtbf.c | Encode text as a brainfuck program
