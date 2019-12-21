
This is a collection of other BF interpreters, compilers, tools and components.

The files bf2c_v1.b and bf2c_v2.b are simple BF to C converters the second does RLE (strangely). The other bf2c*.* are also converters from BF to C written in their respective languages.

Many of the C programs are interpreters, mostly components that were merged into one of my major interpreters or attempts on the theme of "small but tidy".

The files bf2bash.sh, bf2lua.lua and also bf.pl are interpreters that convert to the host language then use an "eval" command.

The txtbf.c program takes a text input and generates a brainfuck program to output that text, in default mode it's quick and effective. In "-max" mode it's very slow and thorough.

File | Description
-----|------------
profilebf.c | A counting brainfuck interpreter, counts and classifies your brainfuck code.
deadbeef.c | Brainfuck interpreter with TWO instruction lookahead, rather quick.
trixy.c | 'Compile' brainfuck to various trivial languages and other tricksy things.
txtbf.c | Encode text as a brainfuck program
bf.sed | Brainfuck interpreter in SED, no input.
bfopt.c | Brainfuck to brainfuck optimiser
bf2c.awk | Convert Brainfuck to C, then compile and run *optimises*
ook.l | Brainfuck interpreter for various trivial brainfuck substitutions including many I've never heard of.
 |  | 
bf2bash.sh | Convert Brainfuck to bash and "eval" it
bf2c-plain.sed | Very plain brainfuck to C in sed
bf2c.py | Convert Brainfuck to plain C
bf2c.sed | Lightly optimising brainfuck to C
bf2c_v1.b | Unoptimised brainfuck to C in brainfuck
bf2c_v2.b | Slightly optimising brainfuck to C in brainfuck
bf2lua.lua | Lightly optimising brainfuck to Lua in Lua, including Lostkng.b
bfdowhile.c | Brainfuck variant that seems (surprisingly) to be Turing complete
bf.pl | Golfed brainfuck to Perl and eval
bf.rb | Golfed brainfuck to Ruby and eval
bf-trace.c | Simple brainfuck interpreter with *full* trace.
byte2byte.c | Encode text as two cell brainfuck.
cdowhile.c | Turing complete brainfuck variant to C and run.
easy.c | [Easy](http://esolangs.org/wiki/Easy) interpreter and 16bit brainfuck.
hellbox-103.b | _Pretty_ Hello world in brainfuck.
hydrogen.c | Early component for tritium.
lightning.c | Brainfuck interpreter using just the GNU-Lightning library (only RLE _optimisation_)
md5.gw.b | Brainfuck program with same md5 hash as md5.hw.b but different output.
md5.hw.b | Brainfuck program with same md5 hash as md5.gw.b but different output.
microbf.c | Nice tiny brainfuck interpreter
neutron.c | Testbed interpreter for brainfuck smart parser.
proton.c | A simple testbed brainfuck interpreter.
txtbf.b | Encode text as a brainfuck program
txtbf.rb | Encode text as a brainfuck program (in one line)
