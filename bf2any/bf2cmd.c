#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MS Batch translation from BF, runs at about 1,000 instructions per second.
 *
 * For ANSI.SYS replacement see: https://github.com/adoxa/ansicon
 */

int do_input = 0;
int do_output = 0;
int loopid = 0;

struct stkdat { struct stkdat * up; int id; } *sp = 0;

int cells_are_ints = sizeof(int)==4;

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	/*
	 *  Note: using just "SETLOCAL" here, this way MEMORY cells from this
	 *  run will be erased at the end, BUT pre-existing cells will not be
	 *  cleared before the run.
	 */
	printf( "%s%d%s",
	    "@ECHO OFF\r\n"
	    "SETLOCAL ENABLEDELAYEDEXPANSION\r\n"
	    "SET PTR=",tapeinit,"\r\n"
	    "FOR /F %%A IN ('\"PROMPT $H &ECHO ON &FOR %%B IN (1) DO REM\"') DO SET BS=%%A\r\n"
	    "FOR /F %%A IN ('\"PROMPT $E &ECHO ON &FOR %%B IN (1) DO REM\"') DO SET ESC=%%A\r\n"
	    );

	puts("SET ASCII=\"#$%%&'()*+,-./0123456789:;<#>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\"\r");

	puts("GOTO :STARTCODE\r");
	puts("\r");

	puts(":put\r");
	puts("SET /A CH=MEMORY%PTR%^&127\r");
	puts("SET /A AC=%CH%-34\r");

	puts("IF %CH% EQU 10 (\r");
	puts("ECHO.\r");
	puts(") ELSE IF %CH% EQU 61 (\r");
	puts("<NUL SET /P=.%BS%=\r");
	puts(") ELSE IF %CH% EQU 127 (\r");
	puts("rem\r");
	puts(") ELSE IF %CH% GTR 34 (\r");
	puts("<NUL SET /P=!ASCII:~%AC%,1!\r");
	puts(") ELSE IF %CH% EQU 32 (\r");
	puts("<NUL SET /P=:%BS% \r");
	puts(") ELSE IF %CH% EQU 33 (\r");
	puts("<NUL SET /P=^^!\r");
	puts(") ELSE IF %CH% EQU 34 (\r");
	puts("<NUL SET /P=\"\"\"\r");
	puts(")\r");

	puts("EXIT /B 0\r");
	puts("\r");
	puts(":STARTCODE\r");
	break;

    case '=': printf("SET /A MEMORY%%PTR%%=%d\r\n", count); break;
    case 'B':
	    if(bytecell) { puts("SET /A MEMORY%PTR%=MEMORY%PTR%^&255\r"); }
	    printf( "SET /A V=MEMORY%%PTR%%\r\n");
	    break;

    case 'M': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+V*%d\r\n", count); break;
    case 'N': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%-V*%d\r\n", count); break;
    case 'S': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+V\r\n"); break;
    case 'Q': printf("IF %%V%% NEQ 0 (SET /A MEMORY%%PTR%%=%d )\r\n", count); break;
    case 'm': printf("IF %%V%% NEQ 0 (SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+V*%d)\r\n", count); break;
    case 'n': printf("IF %%V%% NEQ 0 (SET /A MEMORY%%PTR%%=MEMORY%%PTR%%-V*%d)\r\n", count); break;
    case 's': printf("IF %%V%% NEQ 0 (SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+V)\r\n"); break;

    case 'X': printf("ECHO Abort Infinite Loop & EXIT\n"); break;

    case '+': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+%d\r\n", count); break;
    case '-': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%-%d\r\n", count); break;
    case '<': printf("SET /A PTR=PTR-%d\r\n", count); break;
    case '>': printf("SET /A PTR=PTR+%d\r\n", count); break;

    case '[':
	{
	    struct stkdat * n = malloc(sizeof*n);
	    n->up = sp;
	    sp = n;
	    n->id = ++loopid;

	    if(bytecell) { puts("SET /A MEMORY%PTR%=MEMORY%PTR%^&255\r"); }
	    printf( "SET /A M=MEMORY%%PTR%%\r\n");
	    printf( "IF %%M%% == 0 GOTO LOOP%dE\r\n", n->id);
	    printf( ":LOOP%dB\r\n", n->id);
	}
	break;
    case ']':
	{
	    struct stkdat * n = sp;
	    sp = n->up;

	    if(bytecell) { puts("SET /A MEMORY%PTR%=MEMORY%PTR%^&255\r"); }
	    printf( "SET /A M=MEMORY%%PTR%%\r\n");
	    printf( "IF NOT %%M%% == 0 GOTO LOOP%dB\r\n", n->id);
	    printf( ":LOOP%dE\r\n", n->id);
	    free(n);
	}
	break;

    case '.':
	puts("CALL :put\r");
	do_output = 1;
	break;
    case ',':
	puts("CALL :get\r");
	do_input = 1;
	break;
    case '~':
	break;
    }
}
