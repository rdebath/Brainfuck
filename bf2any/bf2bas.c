#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * VB.NET translation from BF, runs at about 2,000,000,000 instructions per second.
 *
 * bwbasic (-bw) translation from BF, runs at about 100,000 instructions per second.
 * bas-2.3 (-ansi) translation from BF, runs at about 1,500,000 instructions per second.
 * MOLE-Basic-0.7 (-goto|sort) translation from BF, runs at about 400,000 instructions per second.
 *	WHILE/WEND is broken in MOLE Basic.
 *
 * FreeBasic translation from BF, runs at about 680,000,000 instructions per second.
 * FreeBasic (as integer) translation from BF, runs at about 1,200,000,000 instructions per second.
 *
 */

int ind = 0;
int lineno = 0;
int lineinc = 1;
int open_doloop = 0; /* So we don't get an "unused variable" warning */

enum { loop_wend, loop_while, loop_do, loop_goto } loop_style = loop_do;
enum { init_none, init_dim, init_global, init_main } init_style = init_dim;
enum { end_none, end_end, end_system, end_vb } end_style = end_end;
enum { io_basic, io_vb } io_style = io_basic;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

void
line_no_indent(void)
#define I   line_no_indent()
{
    int i = 0;
    lineno += lineinc;
    if (loop_style == loop_goto)
	i = -4 + printf("%-3d ", lineno);
    printf("%*s", ind*4-i, "");
}

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    else
    if (strcmp("-wend", arg) ==0) {
	init_style = init_dim;
	loop_style = loop_wend;
	end_style = end_end;
	io_style = io_basic;
	return 1;
    } else
    if (strcmp("-ansi", arg) ==0) {
	init_style = init_dim;
	loop_style = loop_do;
	end_style = end_end;
	io_style = io_basic;
	return 1;
    } else
    if (strcmp("-vb", arg) ==0) {
	init_style = init_main;
	loop_style = loop_while;
	end_style = end_vb;
	io_style = io_vb;
	return 1;
    } else
    if (strcmp("-goto", arg) ==0) {
	init_style = init_dim;
	loop_style = loop_goto;
	end_style = end_end;
	io_style = io_basic;
	return 1;
    } else
    if (strcmp("-bac", arg) ==0) {
	/* BaCon basic, uses square brackets -- wrong. */
	init_style = init_global;
	loop_style = loop_wend;
	end_style = end_end;
	io_style = io_basic;
	return 1;
    } else
    if (strcmp("-bw", arg) ==0) {
	init_style = init_dim;
	loop_style = loop_wend;
	end_style = end_system;
	io_style = io_basic;
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "default is ansi"
	"\n\t"  "-ansi   ANSI basic (do while style loop)"
	"\n\t"  "-wend   Like ansi but use While ... Wend"
	"\n\t"  "-goto   Basic using GOTOs and line numbers (may need 'sort -n')"
	"\n\t"  "-vb     Microsoft VB.NET"
	"\n\t"  "-bw     BWBasic");
	return 1;
    } else
	return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	lineno = 0;

	switch(init_style) {
	    case init_none:
		break;
	    case init_dim:
		I; printf("DIM M(32700)\n");
		I; printf("P = %d\n", BOFF);
		break;
	    case init_global:
		I; printf("GLOBAL M TYPE int ARRAY 30000\n");
		I; printf("P = %d\n", BOFF);
		break;
	    case init_main:
		I; printf("Public Module MM\n");
		ind++;
		I; printf("Sub Main()\n");
		ind++;
		/* Using "Dim M(32700) As Byte" means we have to fix overflows
		 * rather than having an automatic wrap.
		 */
		I; printf("Dim M(32700) As Integer\n");
		I; printf("Dim P As Integer\n");
		// I; printf("Dim V As Integer ' Sigh, yes I know, sometimes unused.\n");
		I; printf("P = %d\n", BOFF);
		break;
	}
	break;
    case '~':
	switch(end_style) {
	    case end_none:
		break;
	    case end_end:
		I; printf("END\n");
		break;
	    case end_system:
		I; printf("SYSTEM\n");
		break;
	    case end_vb:
		if (open_doloop) {
		    ind--;
		    I; printf("Loop While False\n");
		    open_doloop = 0;
		}
		ind--;
		I; printf("End Sub\n");
		ind--;
		I; printf("End Module\n");
		break;
	}
	break;

    case 'X':
	I; printf("print \"Infinite Loop\"\n");
	I; printf("GOTO 200\n");
	break;
    case '=': I; printf("M(P)=%d\n", count); break;
    case 'B':
	if (end_style == end_vb && !open_doloop) {
	    I; printf("Do\n");
	    ind++;
	    I; printf("Dim V As Integer\n");
	    open_doloop = 1;
	}

	if(bytecell) { I; printf("M(P)=M(P) MOD 256\n"); }
	I; printf("V=M(P)\n");
	break;
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
    case '.':
	switch(io_style) {
	case io_basic:
	    I; printf("PRINT CHR$(M(P));\n"); break;
	case io_vb:
	    I; printf("Console.Write(Chr(M(P)))\n"); break;
	}
	break;
    case ',':
	switch(io_style) {
	case io_basic:
	    I; printf("IF A$ = \"\" THEN\n");
	    ind ++;
	    I; printf("INPUT \"\",A$\n");
	    I; printf("A$ = A$ + CHR$(10)\n");
	    ind --;
	    I; printf("END IF\n");
	    I; printf("M(P) = ASC(A$)\n");
	    I; printf("A$ = MID$(A$,2)\n");
	    break;
	case io_vb:
	    I; printf("Do\n");
	    ind ++;
	    I; printf("Dim C As Integer\n");
	    I; printf("Do\n");
	    ind ++;
	    I; printf("C = Console.Read()\n");
	    ind --;
	    I; printf("Loop While C = 13\n");
	    I; printf("IF C <> -1 THEN M(P)=C\n");
	    ind --;
	    I; printf("Loop While False\n");
	    break;
	}
	break;
    case '[':
	if (open_doloop) {
	    ind--;
	    I; printf("Loop While False\n");
	    open_doloop = 0;
	}

	if(bytecell) { I; printf("M(P)=M(P) MOD 256\n"); }
	switch(loop_style) {
	    case loop_goto:
		{
		    struct stkdat * n = malloc(sizeof*n);
		    n->up = sp;
		    sp = n;
		    n->id = lineno;
		    lineno += lineinc;
		}
		break;
	    case loop_do:
		I; printf("DO WHILE M(P)<>0\n");
		break;
	    case loop_wend:
		I; printf("WHILE M(P)<>0\n");
	    case loop_while:
		I; printf("While M(P)<>0\n");
		break;
	}
	ind++;
	break;
    case ']':
	if (open_doloop) {
	    ind--;
	    I; printf("Loop While False\n");
	    open_doloop = 0;
	}

	if(bytecell) { I; printf("M(P)=M(P) MOD 256\n"); }
	ind--;
	switch(loop_style) {
	    case loop_goto:
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
	    case loop_do:
		I; printf("LOOP\n");
		break;
	    case loop_wend:
		I; printf("WEND\n");
		break;
	    case loop_while:
		I; printf("End While\n");
		break;
	}
	break;
    }
}
