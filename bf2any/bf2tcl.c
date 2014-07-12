#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Tcl translation from BF, runs at about 24,000,000 instructions per second.
 */

int ind = 0;
#define I printf("%*s", ind*4, "")
int tapelen = 30000;

static void print_cstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strncmp(arg, "-M", 2) == 0) {
	tapelen = strtoul(arg+2, 0, 10);
	if (tapelen<1) tapelen = 30000;
	tapelen += BOFF;
	return 1;
    }
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf( "%s%d%s%d%s",
		"#!/usr/bin/tclsh\n"
		"package require Tcl 8.5\n"
		"fconfigure stdout -buffering none\n"
		"fconfigure stdin -buffering none\n"
		"proc run_bf {} {\n"
		"set d [lrepeat ",tapelen," 0]\n"
		"set dc ", BOFF, "\n");
	break;
    case '~':
	puts("}\nrun_bf");
	break;

    case '=': I; printf("lset d $dc %d\n", count); break;
    case 'B':
	if(bytecell) { I; puts("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	I; printf("set v [lindex $d $dc]\n");
	break;

    case 'M': I; printf("lset d $dc [expr {[lindex $d $dc] +$v*%d}]\n", count); break;
    case 'N': I; printf("lset d $dc [expr {[lindex $d $dc] -$v*%d}]\n", count); break;
    case 'S': I; printf("lset d $dc [expr {[lindex $d $dc] +$v}]\n"); break;
    case 'Q': I; printf("if {$v != 0} {lset d $dc %d}\n", count); break;
    case 'm': I; printf("if {$v != 0} {lset d $dc [expr {[lindex $d $dc] +$v*%d}] }\n", count); break;
    case 'n': I; printf("if {$v != 0} {lset d $dc [expr {[lindex $d $dc] -$v*%d}] }\n", count); break;
    case 's': I; printf("if {$v != 0} {lset d $dc [expr {[lindex $d $dc] +$v}] }\n"); break;

#if 0
    case 'X': I; printf("die(\"Abort: Infinite Loop.\\n\");\n"); break;
#endif

    case '+': I; printf("lset d $dc [expr {[lindex $d $dc] + %d}]\n", count); break;
    case '-': I; printf("lset d $dc [expr {[lindex $d $dc] - %d}]\n", count); break;
    case '<': I; printf("incr dc %d\n", -count); break;
    case '>': I; printf("incr dc %d\n", count); break;
    case '[':
	if(bytecell) { I; puts("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	I; printf("while {[lindex $d $dc] != 0} {\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; puts("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	ind--; I; printf("}\n");
	break;
    case '.': I; puts("puts -nonewline [format \"%c\" [lindex $d $dc]]"); break;
    case '"': print_cstring(); break;
    case ',':
	I; puts("set c [scan [read stdin 1] \"%c\"]");
	I; puts("if {$c != {}} {lset d $dc $c}");
	break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		I; printf("puts \"%s\"\n", buf);
	    } else {
		I; printf("puts -nonewline \"%s\"\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '[' ||
	    *str == '}' || *str == '{' || *str == '$') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~') {
	    buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
