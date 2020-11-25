#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * C Shell translation from BF, runs at about 47,000 instructions per second.
 */

static int ind = 0;
#define prv(s,v)        printf("%*s" s "\n", ind*4, "", (v))
#define pr(s)           printf("%*s" s "\n", ind*4, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_string(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	pr("#!/bin/csh");
	pr("# OH! the quoting!!");
	pr("alias outchar 'awk -v c=\\!^ '\"'\"'BEGIN{printf \"%%c\",c;}'\"'\"");

	pr("");
	if (!count) {
	    pr("set M=(0 0) V=0 P=1 E=1 L=()");
	    prv("@ P=%d", tapeinit);
	    pr("while ($P > $E)");
	    pr("    @ E += 1");
	    pr("    set M=( $M 0 )");
	    pr("end");
	}
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
    case 'T': pr("@ M[$P]-=$V"); break;
    case '*': pr("@ M[$P]*=$V"); break;
    case '/': pr("@ M[$P]/=$V"); break;
    case '%': pr("@ M[$P]%%=$V"); break;

    case 'C': prv("@ M[$P]=$V * %d", count); break;
    case 'D': prv("@ M[$P]=-$V * %d", count); break;
    case 'V': pr("@ M[$P]=$V"); break;
    case 'W': pr("@ M[$P]=-$V"); break;

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
    case '"': print_string(strn); break;

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

    case 'I':
	if(bytecell) pr("@ M[$P] = ($M[$P] %% 256 + 256) %% 256");
	pr("if ( $M[$P] != 0 ) then");
	ind++;
	break;
    case 'E':
	ind--;
	pr("endif");
	break;

    case '~':
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n') {
		prv("echo '%s'", buf);
		badchar = 0;
	    } else {
		prv("echo -n '%s'", buf);
	    }
	    outlen = 0;
	}
	if (badchar) {
	    if (badchar == 10)
		pr("echo ''");
	    else
		prv("outchar %d", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '-' && outlen == 0) {
	    badchar = (*str & 0xFF);
	} else if (*str >= ' ' && *str <= '~' && *str != '\'' && *str != '!') {
	    buf[outlen++] = *str;
	} else if (*str == '\'' || *str == '!') {
	    buf[outlen++] = '\'';
	    buf[outlen++] = '\\';
	    buf[outlen++] = *str;
	    buf[outlen++] = '\'';
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
