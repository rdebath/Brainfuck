#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if defined(ENABLE_LIBTCL) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

#ifdef ENABLE_LIBTCL
#include <tcl.h>
#endif

#include "bf2any.h"

/*
 * Tcl translation from BF, runs at about 24,000,000 instructions per second.
 */

static int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")
#define oputs(str) fprintf(ofd, "%s\n", (str))
static int do_dump = 0;
static int hello_world = 0;

static FILE * ofd;
static int use_utf8 = 0;
#ifdef ENABLE_LIBTCL
static char * tclcode = 0;
static size_t tclcodesize = 0;
#endif

static void print_cstring(char * str);

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {.check_arg=fn_check_arg, .gen_code=gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-d") == 0) {
	do_dump = 1;
	return 1;
    } else
#ifdef ENABLE_LIBTCL
    if (strcmp(arg, "-r") == 0) {
	do_dump = 0;
	return 1;
    } else
#endif
    if (strcmp(arg, "-utf8") == 0) {
	use_utf8 = 1;
	return 1;
    } else
    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	hello_world = (count!=0);
#ifdef ENABLE_LIBTCL
	if (!do_dump) {
#ifndef DISABLE_OPENMEMSTREAM
	    ofd = open_memstream(&tclcode, &tclcodesize);
#else
	    ofd = open_tempfile();
#endif
	} else
#endif
	    ofd = stdout;
	fprintf(ofd, "#!/usr/bin/tclsh\n");
	if (!hello_world) {
	    fprintf(ofd, "%s",
		    "package require Tcl 8.5\n"
		    "fconfigure stdout -buffering none\n"
		    "fconfigure stdin -buffering none\n");

	    if (use_utf8) {
		oputs("fconfigure stdin -encoding utf-8");
		oputs("fconfigure stdout -encoding utf-8");
	    }

	    fprintf(ofd, "%s%d%s%d%s",
		    "proc run_bf {} {\n"
		    "set d [lrepeat ",tapesz," 0]\n"
		    "set dc ", tapeinit, "\n");
	}
	break;
    case '~':
	if (!hello_world)
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
    case 'T': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] -$v}]\n"); break;
    case '*': I; fprintf(ofd, "lset d $dc [expr {[lindex $d $dc] *$v}]\n"); break;

    case 'C': I; fprintf(ofd, "lset d $dc [expr { $v*%d}]\n", count); break;
    case 'D': I; fprintf(ofd, "lset d $dc [expr { -$v*%d}]\n", count); break;
    case 'V': I; fprintf(ofd, "lset d $dc $v\n"); break;
    case 'W': I; fprintf(ofd, "lset d $dc [expr { -$v}]\n"); break;

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
    case 'I':
	if(bytecell) { I; oputs("lset d $dc [expr {[lindex $d $dc] % 256}]"); }
	I; fprintf(ofd, "if {[lindex $d $dc] != 0} {\n");
	ind++;
	break;
    case 'E':
	ind--; I; fprintf(ofd, "}\n");
	break;
    case 'X':
	I; fprintf(ofd, "puts stderr \"STOP command executed\"\n");
	I; fprintf(ofd, "exit 255\n");
	break;
    case '.':
	if(bytecell) { I; oputs("lset d $dc [expr {[lindex $d $dc] & 255}]"); }
	I; oputs("puts -nonewline [format %c [lindex $d $dc]]");
	break;
    case '"': print_cstring(strn); break;
    case ',':
	I; oputs("set c [scan [read stdin 1] %c]");
	I; oputs("if {$c != {}} {lset d $dc $c}");
	break;
    }

#ifdef ENABLE_LIBTCL
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
print_cstring(char * str)
{
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
