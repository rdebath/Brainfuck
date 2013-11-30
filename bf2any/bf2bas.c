#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * FreeBasic translation from BF, runs at about 680,000,000 instructions per second.
 * FreeBasic (as integer) translation from BF, runs at about 1,200,000,000 instructions per second.
 *
 * bwbasic (-bw) translation from BF, runs at about 100,000 instructions per second.
 * bas-2.3 (-ansi) translation from BF, runs at about 1,500,000 instructions per second.
 * MOLE-Basic-0.7 (-goto|sort) translation from BF, runs at about 400,000 instructions per second.
 *	WHILE/WEND is broken in MOLE Basic.
 * yabasic translation from BF, runs at about 15,000,000 instructions per second.
 *
 */

extern int bytecell;

int do_input = 0;
int ind = 1;
int lineno = 0;
int lineinc = 1;
int style = 0;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

void
line_no_indent(void)
#define I   line_no_indent()
{
    int i;
    lineno += lineinc;
    if (style != 2) {
	printf("%*s", ind*4, "");
	return;
    }
    i = printf("%-3d ", lineno);
    printf("%*s", ind*4-i, "");
}

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    else
    if (strcmp("-ansi", arg) ==0) {
	style = 1; return 1;
    } else
    if (strcmp("-goto", arg) ==0) {
	style = 2; return 1;
    } else
    if (strcmp("-bac", arg) ==0) {
	style = 10; return 1;
    } else
    if (strcmp("-bw", arg) ==0) {
	style = 11; return 1;
    } else
	return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	lineno = 200 - lineinc;

	switch(style) {
	    default:
	    case 0:
	    case 1:
	    case 2:
		I; printf("DIM M(32700)\n");
		I; printf("P = %d\n", BOFF);
		break;
	    case 10:
		I; printf("GLOBAL M TYPE int ARRAY 30000\n");
		I; printf("P = %d\n", BOFF);
		break;
	}
	break;
    case '~':
	if (do_input) {
	    int savedline = lineno;
	    printf("10  GOTO 200\n");
	    printf("100 IF A$ = \"\" THEN\n");
	    lineno = 100;
	    ind ++;
	    I; printf("INPUT \"\",A$\n");
	    I; printf("A$ = A$ + CHR$(10)\n");
	    ind --;
	    I; printf("END IF\n");
	    I; printf("M(P) = ASC(A$)\n");
	    I; printf("A$ = MID$(A$,2)\n");
	    I; printf("RETURN\n");
	    if (style != 2)
		printf("200 :\n");
	    lineno = savedline;
	}

	switch(style) {
	    default:
	    case 0:
	    case 1:
	    case 2:
		I; printf("END\n");
		break;
	    case 11:
		I; printf("SYSTEM\n");
		break;
	}
	break;

    case 'X': I; printf("print \"Infinite Loop\"\nGOTO 200\n"); break;
    case '=': I; printf("M(P)=%d\n", count); break;
    case 'B': I; printf("V=M(P)\n"); break;
    case 'M': I; printf("M(P)=M(P)+V*%d\n", count); break;
    case 'N': I; printf("M(P)=M(P)-V*%d\n", count); break;
    case 'S': I; printf("M(P)=M(P)+V\n"); break;
    case 'Q': I; printf("IF V<>0 THEN M(P) = %d\n", count); break;

    case 'm': I; printf("IF V<>0 THEN M(P)=M(P)+V*%d\n", count); break;
    case 'n': I; printf("IF V<>0 THEN M(P)=M(P)-V*%d\n", count); break;
    case 's': I; printf("IF V<>0 THEN M(P)=M(P)+V\n"); break;

    case '+': I; printf("M(P)=M(P)+%d\n", count); break;
    case '-': I; printf("M(P)=M(P)-%d\n", count); break;
    case '<': I; printf("P=P-%d\n", count); break;
    case '>': I; printf("P=P+%d\n", count); break;
    case '.': I; printf("PRINT CHR$(M(P));\n"); break;
    case ',': I; printf("GOSUB 100\n"); do_input++; break;
    case '[':
	if(bytecell) { I; printf("M(P)=M(P) MOD 256\n"); }
	switch(style) {
	    case 2:
		{
		    struct stkdat * n = malloc(sizeof*n);
		    n->up = sp;
		    sp = n;
		    n->id = lineno;
		    lineno += lineinc;
		}
		break;
	    default:
	    case 1:
		I; printf("DO WHILE M(P)<>0\n");
		break;
	    case 0:
	    case 10:
		I; printf("WHILE M(P)<>0\n");
		break;
	}
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("M(P)=M(P) MOD 256\n"); }
	ind--;
	switch(style) {
	    case 2:
		{
		    struct stkdat * n = sp;
		    int saveline;
		    sp = n->up;

		    saveline = lineno;
		    lineno = n->id;
		    I; printf("IF M(P)=0 THEN GOTO %d\n", saveline+lineinc*2);
		    lineno = saveline;
		    I; printf("IF M(P)<>0 THEN GOTO %d\n", n->id+lineinc*2);
		    free(n);
		}
		break;
	    default:
	    case 1:
		I; printf("LOOP\n");
		break;
	    case 0:
	    case 10:
		I; printf("WEND\n");
		break;
	}
	break;
    }
}
