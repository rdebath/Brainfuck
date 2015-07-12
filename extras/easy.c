/*****************************************************************************
Easy Interpreter.

This is not exactly as in the 'specification'. I have improved
compatibility with Brainfuck by switching the the input and output
characters back to 'normal' and altered the ':' command so that it
ignores any non-command character.

If a file is specified on the command line the program will therefor
execute this as a normal brainfuck program. If no file is specified
easy commands (and data) will be read from the standard input.

Because of the brainfuck compatibility newlines will not terminate the
program; instead you must give the ':' command an EOF indication.
(or abort the program with too many ']' commands)

Errors are indicated with the command return status as normal.

As a brainfuck interpreter this implementation has a limited tape of
65536 cells each of which is 16bits, both cells and tape wrap.
EOF uses the read() convention. This interpreter is slow.

This input is "Hello World!"

++++++++++[>+++++++>++++++++++>+++>+<<<<-]
>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.

As is this:

,H[.[-],e]llo World!

*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>

FILE *f;
char pgm[4000000];
unsigned short m, mem[65536];

char * bf(char * p, int r) {
    char * s;
    for(;;) {
	switch(*p) {
	case ':': {int c;if((c = getc(f)) == EOF) exit(r); *p=c;}p[1]=':'; continue;
	case '+': mem[m]++; break;
	case '-': mem[m]--; break;
	case '>': m++; break;
	case '<': m--; break;
	case '.': putchar(mem[m]); break;
	case ',': {int a=getchar(); if(a!=EOF) mem[m]=a;} break;
	case ']': return p;
	case '[':
	    s=p+1;
	    if(mem[m]) do p=bf(s,1); while(mem[m]);
	    else {
		int b=0;
		for(p++;*p!=']'||b;) {
		    if (*p!=':') { b += (*p=='[')-(*p==']'); p++; }
		    else if((*p = getc(f)) == EOF) exit(2); else p[1]=':';
		}
	    }
	    if(!r) { *(p=pgm)=':'; continue; }
	    break;
	}
	p++;
    }
}

int main(int argc, char**argv)
{
    if(argc==1) f=stdin; else if(!(f=fopen(argv[1],"r"))) exit(4);
    setbuf(stdout, 0); *pgm=':'; bf(pgm,0); exit(3);
}
