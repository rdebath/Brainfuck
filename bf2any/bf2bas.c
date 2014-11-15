#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * VB.NET translation from BF, runs at about 2,000,000,000 instructions per second.
 *
 * Brandy basic translation from BF, runs at about 15,000,000 instructions per second.
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

int enable_linenos = 0;
int open_doloop = 0; /* So we don't get an "unused variable" warning in VB. */
int do_input = 0;

char tapecell[16]="M(P)";
char tapeptr[16]="P";
char tempcell[16]="V";
char tapename[16]="M";

enum { loop_wend, loop_endw, loop_while, loop_do, loop_goto } loop_style = loop_do;
enum { init_none, init_dim, init_global, init_main } init_style = init_dim;
enum { end_none, end_end, end_system, end_vb } end_style = end_end;
enum { io_basic, io_bbc, io_vb } io_style = io_basic;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

void
line_no_indent(void)
#define I   line_no_indent()
{
    int i = 0;
    lineno += lineinc;
    if (enable_linenos)
	i = -4 + printf("%-3d ", lineno);
    printf("%*s", ind*4-i, "");
}

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    else
    if (strcmp("-bbc", arg) ==0) {
	init_style = init_dim;
	loop_style = loop_endw;
	end_style = end_end;
	io_style = io_bbc;
	strcpy(tapecell, "M%(P%)");
	strcpy(tapeptr, "P%");
	strcpy(tempcell, "V%");
	strcpy(tapename, "M%");
	return 1;
    } else
    if (strcmp("-%", arg) ==0) {
	strcpy(tapecell, "M%(P%)");
	strcpy(tapeptr, "P%");
	strcpy(tempcell, "V%");
	strcpy(tapename, "M%");
	return 1;
    } else
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
	enable_linenos = 1;
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
	"\n\t"  "-bbc    BBC Basic"
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
		I; printf("DIM %s(%d)\n", tapename, tapesz);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		break;
	    case init_global:
		I; printf("GLOBAL %s TYPE int ARRAY %d\n", tapename, tapesz);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		break;
	    case init_main:
		I; printf("Public Module MM\n");
		ind++;
		I; printf("Sub Main()\n");
		ind++;
		/* Using "Dim M(32700) As Byte" means we have to fix overflows
		 * rather than having an automatic wrap.
		 */
		I; printf("Dim %s(%d) As Integer\n", tapename, tapesz);
		I; printf("Dim %s As Integer\n", tapeptr);
		I; printf("%s = %d\n", tapeptr, tapeinit);
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
	if (io_style == io_bbc) {
	    I; printf("DEF PROCPRT(C%%)\n");
	    I; printf("IF C%%=10 THEN PRINT ELSE PRINT CHR$(C%%);\n");
	    I; printf("ENDPROC\n");
	}
	if (do_input && io_style == io_bbc) {
	    I; printf("DEF PROCINP\n");
	    I; printf("IF I%% = 0 THEN\n");
	    ind ++;
	    I; printf("INPUT \"\" A$\n");
	    I; printf("I%% = 1\n");
	    ind --;
	    I; printf("ENDIF\n");

	    I; printf("IF A$ = \"\" THEN\n");
	    ind ++;
	    I; printf("%s = 10\n", tapecell);
	    I; printf("I%% = 0\n");
	    ind --;
	    I; printf("ELSE\n");
	    ind ++;
	    I; printf("%s = ASC(A$)\n", tapecell);
	    I; printf("A$ = MID$(A$,2)\n");
	    ind --;
	    I; printf("ENDIF\n");
	    I; printf("ENDPROC\n");
	}
	break;

    case 'X':
	I; printf("print \"Infinite Loop\"\n");
	I; printf("GOTO 200\n");
	break;
    case '=': I; printf("%s=%d\n", tapecell, count); break;
    case 'B':
	if (end_style == end_vb && !open_doloop) {
	    I; printf("Do\n");
	    ind++;
	    I; printf("Dim %s As Integer\n", tempcell);
	    open_doloop = 1;
	}

	if(bytecell) { I; printf("%s=%s MOD 256\n", tapecell, tapecell); }
	I; printf("%s=%s\n", tempcell, tapecell);
	break;
    case 'M': I; printf("%s=%s+%s*%d\n", tapecell, tapecell, tempcell, count); break;
    case 'N': I; printf("%s=%s-%s*%d\n", tapecell, tapecell, tempcell, count); break;
    case 'S': I; printf("%s=%s+%s\n", tapecell, tapecell, tempcell); break;
    case 'Q': I; printf("IF %s<>0 THEN %s = %d\n", tempcell, tapecell, count); break;

    case 'm': I; printf("IF %s<>0 THEN %s=%s+%s*%d\n", tempcell, tapecell, tapecell, tempcell, count); break;
    case 'n': I; printf("IF %s<>0 THEN %s=%s-%s*%d\n", tempcell, tapecell, tapecell, tempcell, count); break;
    case 's': I; printf("IF %s<>0 THEN %s=%s+%s\n", tempcell, tapecell, tapecell, tempcell); break;

    case '+': I; printf("%s=%s+%d\n", tapecell, tapecell, count); break;
    case '-': I; printf("%s=%s-%d\n", tapecell, tapecell, count); break;
    case '<': I; printf("%s=%s-%d\n", tapeptr, tapeptr, count); break;
    case '>': I; printf("%s=%s+%d\n", tapeptr, tapeptr, count); break;
    case '.':
	switch(io_style) {
	case io_basic:
	    I; printf("IF %s<>10 THEN PRINT CHR$(%s);\n", tapecell, tapecell);
	    I; printf("IF %s=10 THEN PRINT\n", tapecell);
	    break;
	case io_vb:
	    I; printf("Console.Write(Chr(%s))\n", tapecell);
	    break;
	case io_bbc:
	    I; printf("PROCPRT(%s)\n", tapecell);
	    break;
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
	    I; printf("%s = ASC(A$)\n", tapecell);
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
	    I; printf("IF C <> -1 THEN %s=C\n", tapecell);
	    ind --;
	    I; printf("Loop While False\n");
	    break;
	case io_bbc:
	    I; printf("PROCINP\n");
	    do_input = 1;
	    break;
	}
	break;
    case '[':
	if (open_doloop) {
	    ind--;
	    I; printf("Loop While False\n");
	    open_doloop = 0;
	}

	if(bytecell) { I; printf("%s=%s MOD 256\n", tapecell, tapecell); }
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
		I; printf("DO WHILE %s<>0\n", tapecell);
		break;
	    case loop_wend:
		I; printf("WHILE %s<>0\n", tapecell);
		break;
	    case loop_while:
		I; printf("While %s<>0\n", tapecell);
		break;
	    case loop_endw:
		I; printf("WHILE %s<>0\n", tapecell);
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

	if(bytecell) { I; printf("%s=%s MOD 256\n", tapecell, tapecell); }
	ind--;
	switch(loop_style) {
	    case loop_goto:
		{
		    struct stkdat * n = sp;
		    int saveline;
		    sp = n->up;

		    saveline = lineno;
		    lineno = n->id;
		    I; printf("IF %s=0 THEN GOTO %d\n", tapecell, saveline+lineinc*2);
		    lineno = saveline;
		    I; printf("IF %s<>0 THEN GOTO %d\n", tapecell, n->id+lineinc*2);
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
	    case loop_endw:
		I; printf("ENDWHILE\n");
		break;
	}
	break;
    }
}
