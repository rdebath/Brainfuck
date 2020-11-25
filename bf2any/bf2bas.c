#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * FreeBasic translation from BF, runs at about 2,800,000,000 instructions per second.
 * VB.NET translation from BF, runs at about 2,000,000,000 instructions per second.
 *
 * Brandy basic (-bbc) translation from BF, runs at about 15,000,000 instructions per second.
 * bas-2.3 (-ansi) translation from BF, runs at about 1,500,000 instructions per second.
 * MOLE-Basic-0.7 (-mole|sort) translation from BF, runs at about 400,000 instructions per second.
 * bwbasic (-bw) translation from BF, runs at about 100,000 instructions per second.
 *
 * Are these supposed to be the same language!!
 */

static int ind = 0;
static int lineno = 0;
static int lineinc = 1;

static int enable_linenos = 0;
static int do_input = 0;
static int do_output = 0;
static int do_indent = 0;

static char tapecell[16]="M(P)";
static char tapeptr[16]="P";
static char tempcell[16]="V";
static char tapename[16]="M";
static char modop[16]="MOD";
static char divop[16]="\\";

static enum { loop_ansi, loop_wend, loop_endwhile, loop_end_while, loop_yabasic, loop_goto }
    loop_style = loop_ansi;
static enum { init_none, init_ansi, init_bacon, init_vb, init_fbas, init_bas256 }
    init_style = init_ansi;
static enum { end_none, end_ansi, end_system, end_vb }
    end_style = end_ansi;
static enum { io_ansi, io_bbc, io_vb, io_fbas, io_bas256, io_bacon, io_yabasic, io_mole }
    io_style = io_ansi;
static enum { if_ansi, if_endif, if_goto }
    if_style = if_ansi;

static void print_string(char * str);

static struct stkdat { struct stkdat * up; int id; } *sp = 0;

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {.check_arg=fn_check_arg, .gen_code=gen_code};

static void
line_no_indent(void)
#define I   line_no_indent()
{
    int i = 0;
    lineno += lineinc;
    if (enable_linenos && do_indent >= 0)
	i = -4 + printf("%-3d ", lineno);
    else if (enable_linenos)
	printf("%d ", lineno);
    if (do_indent >= 0)
	printf("%*s", ind*4-i, "");
}

static int
fn_check_arg(const char * arg)
{
    if (strcmp("-bbc", arg) ==0 || strcmp("-brandy", arg) ==0) {
	strcpy(divop, "/");
	init_style = init_ansi;
	loop_style = loop_endwhile;
	end_style = end_ansi;
	io_style = io_bbc;
	if_style = if_endif;
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
	loop_style = loop_wend;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-ansi", arg) ==0) {
	strcpy(divop, "\\");
	init_style = init_ansi;
	loop_style = loop_ansi;
	end_style = end_ansi;
	io_style = io_ansi;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-vb", arg) ==0) {
	strcpy(divop, "/");
	init_style = init_vb;
	loop_style = loop_end_while;
	end_style = end_vb;
	io_style = io_vb;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-yabasic", arg) ==0) {
	loop_style = loop_yabasic;
	io_style = io_yabasic;
	if_style = if_endif;
	return 1;
    } else
    if (strcmp("-goto", arg) ==0) {
	strcpy(divop, "\\");
	init_style = init_ansi;
	loop_style = loop_goto;
	end_style = end_ansi;
	io_style = io_ansi;
	if_style = if_goto;
	enable_linenos = 1;
	if (do_indent == 0) do_indent--;
	return 1;
    } else
    if (strcmp("-mole", arg) ==0) {
	strcpy(divop, "");
	init_style = init_ansi;
	loop_style = loop_goto;
	end_style = end_ansi;
	io_style = io_mole;
	if_style = if_goto;
	enable_linenos = 1;
	if (do_indent == 0) do_indent--;
	return 1;
    } else
    if (strcmp("-bacon", arg) ==0) {
	/* BaCon basic, uses square brackets -- wrong. */
	strcpy(tapecell, "M[P]");
	strcpy(divop, "/");
	strcpy(modop, "%");
	init_style = init_bacon;
	loop_style = loop_wend;
	end_style = end_ansi;
	io_style = io_bacon;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-bw", arg) ==0) {
	strcpy(divop, ""); /* There isn't one, use MOD expression */
	init_style = init_ansi;
	loop_style = loop_ansi;
	end_style = end_system;
	io_style = io_ansi;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-free", arg) ==0) {
	strcpy(divop, "\\");
	init_style = init_fbas;
	loop_style = loop_wend; /* Also loop_ansi */
	end_style = end_none;
	io_style = io_fbas;
	if_style = if_endif; /* Also if_ansi */
	return 1;
    } else
    if (strcmp("-bas256", arg) ==0) {
	/* basic256 basic, uses square brackets -- wrong. */
	strcpy(tapecell, "M[P]");
	strcpy(divop, "/");
	strcpy(modop, "%");
	init_style = init_bas256;
	loop_style = loop_endwhile;
	end_style = end_ansi;
	io_style = io_bas256;
	if_style = if_ansi;
	return 1;
    } else
    if (strcmp("-indent", arg) ==0) {
	do_indent = 1;
	return 1;
    } else
    if (strcmp("-no-indent", arg) ==0) {
	do_indent = -1;
	return 1;
    } else
    if (strcmp("-no-intdiv", arg) ==0) {
	strcpy(divop, "");
	return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "default is ansi"
	"\n\t"  "-ansi       ANSI basic (do while style loop)"
	"\n\t"  "-free       Free BASIC"
	"\n\t"  "-bbc        BBC Basic (Also -brandy)"
	"\n\t"  "-wend       Like ansi but use While ... Wend"
	"\n\t"  "-goto       Basic using GOTOs and line numbers (may need 'sort -n')"
	"\n\t"  "-vb         Microsoft VB.NET"
	"\n\t"  "-bw         BWBasic"
	"\n\t"  "-bacon      BaCon"
	"\n\t"  "-bas256     Basic256"
	"\n\t"  "-%          Add a '%' suffix to variables"
	"\n\t"  "-no-indent  Don't indent the code."
	"\n\t"  "-no-intdiv  Don't use integer div operation."
	);
	return 1;
    } else
	return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	lineno = 0;

	switch(init_style) {
	    case init_none:
		break;
	    case init_ansi:
		I; printf("DIM %s(%d)\n", tapename, tapesz);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		break;
	    case init_bacon:
		I; printf("GLOBAL %s TYPE int ARRAY %d\n", tapename, tapesz);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		break;
	    case init_vb:
		I; printf("Public Module MM\n");
		ind++;
		I; printf("Sub Main()\n");
		ind++;
		/* Using "Dim M(32700) As Byte" means we have to fix overflows
		 * rather than having an automatic wrap.
		 */
		I; printf("Dim %s(%d) As Integer\n", tapename, tapesz);
		I; printf("Dim %s As Integer = 0\n", tempcell);
		I; printf("Dim %s As Integer\n", tapeptr);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		break;
	    case init_fbas:
		if (bytecell) {
		    I; printf("Dim %s() As UByte\n", tapename);
		} else {
		    I; printf("Dim %s() As UInteger\n", tapename);
		}
		I; printf("ReDim %s(%d)\n", tapename, tapesz);
		I; printf("Dim %s As Integer\n", tapeptr);
		I; printf("Dim %s As UInteger\n", tempcell);
		I; printf("Dim shared InpStr As string\n");
		I; printf("%s = %d\n", tapeptr, tapeinit);
		printf("\n");
		I; printf("Sub PrtCh(ch as integer)\n");
		ind++;
		I; printf("IF ch<>10 THEN PRINT CHR$(ch); ELSE PRINT END IF\n");
		ind--;
		I; printf("End Sub\n");
		printf("\n");

		/* This is a hack, and it still doesn't work properly
		 * It seems FreeBASIC trys to be "smart" with stdin
		 * but it end up being depressingly stupid AND buggy.
		 */
		I; printf("Open Cons For Input As #1\n");
		if (bytecell) {
		    I; printf("Sub InpCh(ByRef ch as UByte)\n");
		} else {
		    I; printf("Sub InpCh(ByRef ch as integer)\n");
		}
		ind++;
		I; printf("IF InpStr = \"\" THEN\n");
		ind ++;
		I; printf("IF EOF(1) THEN\n");
		ind ++;
		I; printf("EXIT SUB\n");
		ind --;
		I; printf("END IF\n");
		I; printf("INPUT \"\", InpStr\n");
		I; printf("InpStr = InpStr + CHR$(10)\n");
		ind --;
		I; printf("END IF\n");
		I; printf("ch = ASC(InpStr)\n");
		I; printf("InpStr = MID$(InpStr,2)\n");

		ind --;
		I; printf("End Sub\n");
		printf("\n");
		break;
	    case init_bas256:
		I; printf("DIM %s(%d)\n", tapename, tapesz);
		I; printf("%s = %d\n", tapeptr, tapeinit);
		I; printf("A$ = \"\"\n");
		break;
	}
	break;
    case '~':
	switch(end_style) {
	    case end_none:
		break;
	    case end_ansi:
		I; printf("END\n");
		break;
	    case end_system:
		I; printf("SYSTEM\n");
		break;
	    case end_vb:
		ind--;
		I; printf("End Sub\n");
		ind--;
		I; printf("End Module\n");
		break;
	}
	if (do_output && io_style == io_bbc) {
	    I; puts("DEF PROCPRT(C%)");
	    I; puts("IF C%=10 THEN PRINT ELSE PRINT CHR$(C%);");
	    I; puts("ENDPROC");
	}
	if (do_input && io_style == io_bbc) {
	    I; puts("DEF PROCINP");
	    I; puts("IF I% = 0 THEN");
	    ind ++;
	    I; puts("INPUT \"\" A$");
	    I; puts("I% = 1");
	    ind --;
	    I; puts("ENDIF");

	    I; puts("IF A$ = \"\" THEN");
	    ind ++;
	    I; printf("%s%s\n", tapecell," = 10");
	    I; puts("I% = 0");
	    ind --;
	    I; puts("ELSE");
	    ind ++;
	    I; printf("%s%s\n", tapecell," = ASC(A$)");
	    I; puts("A$ = MID$(A$,2)");
	    ind --;
	    I; puts("ENDIF");
	    I; puts("ENDPROC");
	}
	break;

    case 'X':
	if (init_style == init_vb) {
	    I; printf("Console.Write(\"Infinite Loop\")\n");
	    I; printf("Environment.Exit(1)\n");
	} else if (init_style == init_bas256) {
	    I; printf("PRINT \"Infinite Loop\"\n");
	    I; printf("END\n");
	} else {
	    I; printf("PRINT \"Infinite Loop\"\n");
	    I; printf("STOP\n");
	}
	break;
    case '=': I; printf("%s=%d\n", tapecell, count); break;
    case 'B':
	if(bytecell && init_style != init_fbas) {
	    I; printf("%s=%s %s 256\n", tapecell, tapecell, modop);
	}
	I; printf("%s=%s\n", tempcell, tapecell);
	break;
    case 'M': I; printf("%s=%s+%s*%d\n", tapecell, tapecell, tempcell, count); break;
    case 'N': I; printf("%s=%s-%s*%d\n", tapecell, tapecell, tempcell, count); break;
    case 'S': I; printf("%s=%s+%s\n", tapecell, tapecell, tempcell); break;
    case 'T': I; printf("%s=%s-%s\n", tapecell, tapecell, tempcell); break;
    case '*': I; printf("%s=%s*%s\n", tapecell, tapecell, tempcell); break;
    case '/':
	if (*divop) {
	    I; printf("%s=%s %s %s\n", tapecell, tapecell, divop, tempcell);
	} else {
	    I; printf("%s%s%s%s%s%s%s%s%s%s%s\n",
		tapecell,"= (",tapecell," - (",tapecell," ",modop," ",tempcell,")) / ",tempcell
		);
	}
	break;
    case '%': I; printf("%s=%s %s %s\n", tapecell, tapecell, modop, tempcell); break;

    case 'C': I; printf("%s=%s*%d\n", tapecell, tempcell, count); break;
    case 'D': I; printf("%s=-%s*%d\n", tapecell, tempcell, count); break;
    case 'V': I; printf("%s=%s\n", tapecell, tempcell); break;
    case 'W': I; printf("%s=-%s\n", tapecell, tempcell); break;

    case '+': I; printf("%s=%s+%d\n", tapecell, tapecell, count); break;
    case '-': I; printf("%s=%s-%d\n", tapecell, tapecell, count); break;
    case '<': I; printf("%s=%s-%d\n", tapeptr, tapeptr, count); break;
    case '>': I; printf("%s=%s+%d\n", tapeptr, tapeptr, count); break;
    case '.':
	switch(io_style) {
	case io_ansi: case io_mole: case io_bacon:
	    I; printf("IF %s<>10 THEN PRINT CHR$(%s);\n", tapecell, tapecell);
	    I; printf("IF %s=10 THEN PRINT\n", tapecell);
	    break;
	case io_vb:
	    I; printf("Console.Write(Chr(%s))\n", tapecell);
	    break;
	case io_bbc:
	    I; printf("PROCPRT(%s)\n", tapecell);
	    do_output = 1;
	    break;
	case io_fbas:
	    I; printf("PrtCh(%s)\n", tapecell);
	    do_output = 1;
	    break;
	case io_bas256:
	    I; printf("IF %s <> 10 THEN PRINT CHR(%s);\n", tapecell, tapecell);
	    I; printf("IF %s = 10 THEN PRINT\n", tapecell);
	    break;
	case io_yabasic:
	    I; printf("IF %s <> 10 THEN PRINT CHR$(%s); : ENDIF\n", tapecell, tapecell);
	    I; printf("IF %s = 10 THEN PRINT : ENDIF\n", tapecell);
	    break;
	}
	break;
    case '"': print_string(strn); break;
    case ',':
	switch(io_style) {
	case io_ansi: case io_mole:
	    if (if_style == if_goto) {

		int if_lno, saveline;
		if_lno = lineno;
		lineno += lineinc;
		ind ++;
		if (io_style == io_mole) {
		    I; printf("INPUT A$\n");
		    I; printf("A$ = A$ + CHR$(10)\n");
		} else {
		    I; printf("INPUT \"\",A$\n");
		    I; printf("A$ = A$ + CHR$(10)\n");
		}
		ind --;
		saveline = lineno;
		lineno = if_lno;
		I; printf("IF A$ <> \"\" THEN GOTO %d\n", saveline+lineinc*2);
		lineno = saveline;
		I; printf("REM END IF %d\n", if_lno+lineinc*2);
		I; printf("%s = ASC(A$)\n", tapecell);
		I; printf("A$ = MID$(A$,2)\n");

	    } else {

		I; printf("IF A$ = \"\" THEN\n");
		ind ++;
		/* NB: Comma means "don't print a '?'" */
		I; printf("INPUT \"\",A$\n");
		I; printf("A$ = A$ + CHR$(10)\n");
		ind --;
		I; printf("END IF\n");
		I; printf("%s = ASC(A$)\n", tapecell);
		I; printf("A$ = MID$(A$,2)\n");

	    }
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
        case io_fbas:
	    I; printf("InpCh(%s)\n", tapecell);
            break;
	case io_bas256:
	    I; printf("IF A$ = \"\" THEN\n");
	    ind ++;
	    I; printf("INPUT \"\",A$\n");
	    I; printf("A$ = A$ + CHR(10)\n");
	    ind --;
	    I; printf("END IF\n");
	    I; printf("%s = ASC(A$)\n", tapecell);
	    I; printf("A$ = RIGHT(A$,LENGTH(A$)-1)\n"); /* Nope: MID$ */
	    break;
	case io_bacon:
	    I; printf("IF A$ = \"\" THEN\n");
	    ind ++;
	    I; printf("INPUT \"\",A$\n");
	    I; printf("A$ = A$ & CHR$(10)\n"); /* Note '&' operator */
	    ind --;
	    I; printf("END IF\n");
	    I; printf("%s = ASC(A$)\n", tapecell);
	    I; printf("A$ = MID$(A$,2)\n");
	    break;
	case io_yabasic:
	    I; printf("IF A$ = \"\" THEN\n");
	    ind ++;
	    I; printf("INPUT \"\"  A$\n"); /* No ',' flag */
	    I; printf("A$ = A$ + CHR$(10)\n");
	    ind --;
	    I; printf("ENDIF\n");
	    I; printf("%s = ASC(A$)\n", tapecell);
	    I; printf("A$ = MID$(A$,2)\n");
	    break;
	}
	break;
    case '[':
	if(bytecell && init_style != init_fbas) {
	    I; printf("%s=%s %s 256\n", tapecell, tapecell, modop);
	}
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
	    case loop_ansi:
		I; printf("DO WHILE %s<>0\n", tapecell);
		break;
	    case loop_wend:
		I; printf("WHILE %s<>0\n", tapecell);
		break;
	    case loop_end_while:
		I; printf("While %s<>0\n", tapecell);
		break;
	    case loop_endwhile:
		I; printf("WHILE %s<>0\n", tapecell);
		break;
	    case loop_yabasic:
		I; printf("WHILE (%s<>0)\n", tapecell);
	}
	ind++;
	break;
    case ']':
	if(bytecell && init_style != init_fbas) {
	    I; printf("%s=%s %s 256\n", tapecell, tapecell, modop);
	}
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
	    case loop_ansi:
		I; printf("LOOP\n");
		break;
	    case loop_wend:
		I; printf("WEND\n");
		break;
	    case loop_end_while:
		I; printf("End While\n");
		break;
	    case loop_endwhile:
		I; printf("ENDWHILE\n");
		break;
	    case loop_yabasic:
		I; printf("WEND\n");
	}
	break;
    case 'I':
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
	    default:
		I; printf("IF %s <> 0 THEN\n", tapecell);
		break;
	}
	ind++;
	break;
    case 'E':
	ind--;
	switch(if_style) {
	    case if_goto:
		{
		    struct stkdat * n = sp;
		    int saveline;
		    sp = n->up;

		    saveline = lineno;
		    lineno = n->id;
		    I; printf("IF %s=0 THEN GOTO %d\n", tapecell, saveline+lineinc*2);
		    lineno = saveline;
		    I; printf("REM END IF %d\n", n->id+lineinc*2);
		    free(n);
		}
		break;
	    case if_endif:
		I; printf("ENDIF\n");
		break;
	    default:
		I; printf("END IF\n");
		break;
	}
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int gotnl = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    if (gotnl) {
		buf[--outlen] = 0;
		if (outlen == 0) {
		    I; printf("PRINT\n");
		} else {
		    I; printf("PRINT \"%s\"\n", buf);
		}
	    } else {
		buf[outlen] = 0;
		I; printf("PRINT \"%s\";\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    switch(io_style) {
	    case io_vb: I; printf("Console.Write(Chr(%d))\n", badchar); break;
	    case io_bas256: I; printf("PRINT CHR(%d);\n", badchar); break;
	    default: I; printf("PRINT CHR$(%d);\n", badchar); break;
	    }
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '\n') {
	    gotnl = 1;
	    buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '"') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
