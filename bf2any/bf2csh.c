#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C Shell translation from BF, runs at about 47,000 instructions per second.
 */

int do_input = 0;
int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("#!/bin/csh");
	pr("set M=(0) V=0 P=15");
	pr("while ( $P > 0 )");
	pr("    @ P -= 1");
	pr("    set M=( $M $M )");
	pr("end");
	pr("");
	pr("# OH! the quoting!!");
	pr("alias outchar 'awk -v c=\\!^ '\"'\"'BEGIN{printf \"%%c\",c;}'\"'\"");
	pr("");

#ifdef __linux__
	/* The -N option only works properly on Linux */
	pr("alias getch 'set M[$P]=`od -A n -N 1 -w1 -t u1` || exit'");
#else
	pr("dd status=none count=1 bs=1 </dev/null >&/dev/null");
	pr("if ( $status == 0 ) then");
	pr("    # Can't use a plain 'dd' command because we can't dump stderr");
	pr("    alias getch 'set M[$P]=`dd status=none count=1 bs=1 | od -A n -t u1` || exit'");
	pr("else");
	pr("    alias getch 'sh -c '\"'\"'echo The getch command is broken. 1>&2'\"'\"'; exit'");
	pr("endif");
#endif

	pr("");
	prv("@ P=%d", tapeinit);
	break;

    case 'X': pr("sh -c 'echo Infinite Loop 1>&2' ; exit 1"); break;

    case '=': prv("@ M[$P]=%d", count); break;
    case 'B':
	if(bytecell) pr("@ M[$P] = ($M[$P] %% 256 + 256) %% 256");
	pr("@ V=$M[$P]");
	break;
    case 'M': prv("@ M[$P]+=$V * %d", count); break;
    case 'N': prv("@ M[$P]-=$V * %d", count); break;
    case 'S': pr("@ M[$P]+=$V"); break;
    case 'Q': prv("if ($V != 0) @ M[$P]=%d", count); break;
    case 'm': prv("if ($V != 0) @ M[$P]+=$V * %d", count); break;
    case 'n': prv("if ($V != 0) @ M[$P]-=$V * %d", count); break;
    case 's': pr("if ($V != 0) @ M[$P]+=$V"); break;

    case '+': prv("@ M[$P]+=%d", count); break;
    case '-': prv("@ M[$P]-=%d", count); break;
    case '>': prv("@ P+=%d", count); break;
    case '<': prv("@ P-=%d", count); break;
    case '.': pr("outchar $M[$P]"); break;
    case ',': pr("getch"); do_input++; break;

    case '[':
	if(bytecell) pr("@ M[$P] = ($M[$P] %% 256 + 256) %% 256");
	pr("while ( $M[$P] != 0 )");
	ind++;
	break;
    case ']':
	if(bytecell) pr("@ M[$P] = ($M[$P] %% 256 + 256) %% 256");
	ind--;
	pr("end");
	break;

    case '~':
	break;
    }
}
