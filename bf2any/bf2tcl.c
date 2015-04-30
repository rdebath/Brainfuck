#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(DISABLE_LIBTCL) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

#ifndef DISABLE_LIBTCL
#include <tcl.h>
#endif

#include "bf2any.h"

/*
 * Tcl translation from BF, runs at about 24,000,000 instructions per second.
 */

int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")
#define oputs(str) fprintf(ofd, "%s\n", (str))
int do_dump = 0;

FILE * ofd;
char * tclcode = 0;
size_t tclcodesize = 0;

static void print_cstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-d") == 0) {
	do_dump = 1;
	return 1;
    } else
#ifndef DISABLE_LIBTCL
    if (strcmp(arg, "-r") == 0) {
	do_dump = 0;
	return 1;
    } else
#endif
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
#ifndef DISABLE_LIBTCL
	if (!do_dump) {
#ifndef DISABLE_OPENMEMSTREAM
	    ofd = open_memstream(&tclcode, &tclcodesize);
#else
	    ofd = open_tempfile();
#endif
	} else
#endif
	    ofd = stdout;
	fprintf(ofd, "%s%d%s%d%s",
		"#!/usr/bin/tclsh\n"
		"package require Tcl 8.5\n"
		"fconfigure stdout -buffering none\n"
		"fconfigure stdin -buffering none\n"
		"proc run_bf {} {\n"
		"set d [lrepeat ",tapesz," 0]\n"
		"set dc ", tapeinit, "\n");
	break;
    case '~':
	oputs("}\nrun_bf");
	break;

    case '=': I; fprintf(ofd, "lset d $dc %d\n", count); break;
    case 'B':
	if(bytecell) { I; oputs("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	I; fprintf(ofd, "set v [lindex $d $dc]\n");
	break;

    case 'M': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] +$v*%d}]\n", count); break;
    case 'N': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] -$v*%d}]\n", count); break;
    case 'S': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] +$v}]\n"); break;
    case 'Q': I; fprintf(ofd, "if {$v != 0} {lset d $dc %d}\n", count); break;
    case 'm': I; fprintf(ofd, "if {$v != 0} {lset d $dc [expr {[lindex $d $dc] +$v*%d}] }\n", count); break;
    case 'n': I; fprintf(ofd, "if {$v != 0} {lset d $dc [expr {[lindex $d $dc] -$v*%d}] }\n", count); break;
    case 's': I; fprintf(ofd, "if {$v != 0} {lset d $dc [expr {[lindex $d $dc] +$v}] }\n"); break;

#if 0
    case 'X': I; fprintf(ofd, "die(\"Abort: Infinite Loop.\\n\");\n"); break;
#endif

    case '+': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] + %d}]\n", count); break;
    case '-': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] - %d}]\n", count); break;
    case '<': I; fprintf(ofd, "incr dc %d\n", -count); break;
    case '>': I; fprintf(ofd, "incr dc %d\n", count); break;
    case '[':
	if(bytecell) { I; oputs("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	I; fprintf(ofd, "while {[lindex $d $dc] != 0} {\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; oputs("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	ind--; I; fprintf(ofd, "}\n");
	break;
    case '.': I; oputs("puts -nonewline [format \"%c\" [lindex $d $dc]]"); break;
    case '"': print_cstring(); break;
    case ',':
	I; oputs("set c [scan [read stdin 1] \"%c\"]");
	I; oputs("if {$c != {}} {lset d $dc $c}");
	break;
    }

#ifndef DISABLE_LIBTCL
    if (!do_dump && ch == '~') {
#ifndef DISABLE_OPENMEMSTREAM
	fclose(ofd);
#else
        reload_tempfile(ofd, &tclcode, &tclcodesize);
#endif

	/* The bare interpreter method. Works fine for BF code.
	 */
	Tcl_Interp * interp = Tcl_CreateInterp();
	Tcl_EvalEx(interp, tclcode, -1, 0);
	Tcl_DeleteInterp(interp);
    }
#endif
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
		I; fprintf(ofd, "puts \"%s\"\n", buf);
	    } else {
		I; fprintf(ofd, "puts -nonewline \"%s\"\n", buf);
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
