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

int disable_savestring = 1;

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	pr("#!/bin/csh");
	pr("set M=(0 0) V=0 P=1 E=1 L=()");
	pr("");
	pr("# OH! the quoting!!");
	pr("alias outchar 'awk -v c=\\!^ '\"'\"'BEGIN{printf \"%%c\",c;}'\"'\"");

	pr("");
	prv("@ P=%d", tapeinit);
	pr("while ($P > $E)");
	pr("    @ E += 1");
	pr("    set M=( $M 0 )");
	pr("end");
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
    case '>':
	prv("@ P+=%d", count);
	pr("while ($P > $E)");
	pr("    @ E += 1");
	pr("    set M=( $M 0 )");
	pr("end");
	break;
    case '<': prv("@ P-=%d", count); break;
    case '.': pr("outchar $M[$P]"); break;
    case ',':
	    pr( "if ($#L == 0) set L=( `echo \"$<\" | awk 'BEGIN { "
		"while(1) { if (\\\\!gotline) { gotline = getline ; "
		"if(\\\\!gotline) break ; line = $0 ; } if (line == "
		"\"\") { gotline=0 ; print 10 ; break ; } if (\\\\!genord) "
		"{ for(i=1; i<256; i++) ord[sprintf(\"%%c\",i)] = i ; "
		"genord=1 ; } c = substr(line, 1, 1) ; line=substr(line, 2) "
		"; print ord[c] ; } exit 0 ; } ' ` )");
	    pr("if ($#L > 0 ) then");
	    pr("    set M[$P]=$L[1]");
	    pr("    shift L");
	    pr("endif");
	    break;

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
