#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * MS Batch translation from BF, runs at less than 1,000 instructions per second.
 *
 * For ANSI.SYS replacement see: https://github.com/adoxa/ansicon
 */

int do_input = 0;
int do_output = 0;
int ind = 0;
int loopid = 0;
int tapelen = 1000;

int
check_arg(const char * arg)
{
    if (strncmp(arg, "-M", 2) == 0) {
	tapelen = strtoul(arg+2, 0, 10) + (enable_optim?BOFF:0);
	return 1;
    } else
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf( "%s%d%s%d%s",
	    "@ECHO OFF\r\n"
	    "SET MEMSIZE=",tapelen,"\r\n"
	    "SET /A I_=MEMSIZE-1\r\n"
	    "FOR /L %%P IN (0,1,%I_%) DO SET MEMORY%%P=0\r\n"
	    "SET PTR=",enable_optim?BOFF:0,"\r\n"
	    "SET OUTS=\r\n"
	    );
	break;

    case 'X': printf("ECHO Abort Infinite Loop & EXIT\n"); break;

    case '+': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%+%d\r\n", count); break;
    case '-': printf("SET /A MEMORY%%PTR%%=MEMORY%%PTR%%-%d\r\n", count); break;
    case '<': printf("SET /A PTR=PTR-%d\r\n", count); break;
    case '>': printf("SET /A PTR=PTR+%d\r\n", count); break;
    case '[':
	if(bytecell) { puts("SET /A MEMORY%PTR%=MEMORY%PTR%^&255\r"); }
	loopid++;
	printf( ":LOOP%d\r\n", loopid);
	if(bytecell) { puts("SET /A MEMORY%PTR%=MEMORY%PTR%^&255\r"); }
	printf( "SET /A M=MEMORY%%PTR%%\r\n"
		"IF NOT %%M%% == 0 (\r\n"
		"CALL :WHILE%d\r\n"
		"GOTO :LOOP%d\r\n"
		":WHILE%d\r\n",
		loopid,
		loopid,
		loopid);
	ind++;
	break;
    case ']':
	ind--; printf("GOTO :EOF\r\n)\r\n");
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
	if (do_output) {
	    puts("IF NOT \"%OUTS%\" == \"\" ECHO.%OUTS%\r");
	    puts("GOTO :EOF\r");
	    puts(":put\r");
	    puts("SET /A CH=MEMORY%PTR%^&127\r");
	    puts("IF \"%CH%\"==\"10\" (\r");
	    puts("ECHO.%OUTS%\r");
	    puts("SET OUTS=\r");
	    { int i;
		for(i=' '; i<'~'; i++) {
		    if (i =='%' || i=='"' || i=='&' || i=='^' || i=='|'
			|| i=='>' || i=='<' || i=='(' || i==')') {
			printf(") ELSE IF \"%%CH%%\"==\"%d\" (\r\n", i);
			printf("SET \"OUTS=%%OUTS%%^%c\"\r\n", i);
		    } else {
			printf(") ELSE IF \"%%CH%%\"==\"%d\" (\r\n", i);
			printf("SET \"OUTS=%%OUTS%%%c\"\r\n", i);
		    }
		}
	    }
	    printf(") ELSE IF \"%%CH%%\"==\"%d\" (\r\n", 27);
	    printf("SET \"OUTS=%%OUTS%%%c\"\r\n", 27);
	    puts(")\r");
	}
	puts("GOTO :EOF\r");
	break;
    }
}
