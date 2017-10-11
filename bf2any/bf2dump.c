#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

void
outcmd(int ch, int count)
{
static int imov = 0, ind = 0;

    if (imov && ch != '>' && ch != '<') {
        int mov = imov;

	switch(ch) {
	case '=': case 'B': case 'M': case 'N': case 'S': case 'Q':
	case 'm': case 'n': case 's': case '+': case '-':

	    printf("%c %3d: %*s", ch, count, ind*2, "");
	    switch(ch) {
	    case '=': printf("m[%d] = %d\n", mov, count); return;
	    case 'B': printf("v = m[%d]\n", mov); return;
	    case 'M': printf("m[%d] += v*%d\n", mov, count); return;
	    case 'N': printf("m[%d] -= v*%d\n", mov, count); return;
	    case 'S': printf("m[%d] += v\n", mov); return;
	    case 'Q': printf("if(v!=0) m[%d] = %d\n", mov, count); return;
	    case 'm': printf("if(v!=0) m[%d] += v*%d\n", mov, count); return;
	    case 'n': printf("if(v!=0) m[%d] -= v*%d\n", mov, count); return;
	    case 's': printf("if(v!=0) m[%d] += v\n", mov); return;
	    case '+': printf("m[%d] +=%d\n", mov, count); return;
	    case '-': printf("m[%d] -=%d\n", mov, count); return;
	    }
	}

	if (mov) {
	    imov = 0;
	    if (mov > 0)
		printf("> %3d: %*sm += %d\n", mov, ind*2, "", mov);
	    else if (mov < 0)
		printf("< %3d: %*sm -= %d\n", -mov, ind*2, "", -mov);
	}
    }

    switch(ch) {
    case '>': imov += count; return;
    case '<': imov -= count; return;
    }
    if (ch == ']') ind--;

    printf("%c %3d: %*s", ch, count, ind*2, "");
    switch(ch) {
    case '=': printf("*m = %d\n", count); break;
    case 'B': printf("v = *m\n"); break;
    case 'M': printf("*m += v*%d\n", count); break;
    case 'N': printf("*m -= v*%d\n", count); break;
    case 'S': printf("*m += v\n"); break;
    case 'Q': printf("if(v!=0) *m = %d\n", count); break;
    case 'm': printf("if(v!=0) *m += v*%d\n", count); break;
    case 'n': printf("if(v!=0) *m -= v*%d\n", count); break;
    case 's': printf("if(v!=0) *m += v\n"); break;

    case '+': printf("*m +=%d\n", count); break;
    case '-': printf("*m -=%d\n", count); break;

    case '[': printf("while *m!=0\n"); break;
    case ']': printf("endwhile\n"); break;

    case '.': printf("putch(*m)\n"); break;
    case ',': printf("getch(*m)\n"); break;

    case '"':
	{
	    char * s = get_string();
	    printf("printf(\"");
	    for(;*s;s++)
		if (*s>=' ' && *s<='~' && *s!='\\')
		    putchar(*s);
		else if (*s == '\n')
		    printf("\\n");
		else if (*s == '\\')
		    printf("\\\\");
		else
		    printf("\\%03o", *s);
	    printf("\");\n");
	}
	break;

    default:
	printf("\n");
	break;
    }
    if (ch == '[') ind++;
}
