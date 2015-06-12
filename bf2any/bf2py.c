#ifndef DISABLE_LIBPY
/* This must be first; hopefully nothing else "must be first" too */
#include <Python.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/* Ah, yes, this should be first too ... idiots. */
#if !defined(DISABLE_LIBPY) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

/*
 * Python translation from BF, runs at about 18,000,000 instructions per second.
 *
 * There is a limit on the number of nested loops was 20 now 100.
 */

int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")

int do_dump = 0;
int use_syslib = 1;

FILE * ofd;
char * pycode = 0;
size_t pycodesize = 0;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-M") == 0) {
	tapelen = 0;
	return 1;
    }
    if (strcmp(arg, "-d") == 0) {
	do_dump = 1;
	return 1;
    } else
#ifndef DISABLE_LIBPY
    if (strcmp(arg, "-r") == 0) {
	do_dump = 0;
	return 1;
    } else
#endif
    if (strcmp(arg, "-oslib") == 0) {
	use_syslib = 0;
	return 1;
    } else
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
#ifndef DISABLE_LIBPY
        if (!do_dump) {
#ifndef DISABLE_OPENMEMSTREAM
	    ofd = open_memstream(&pycode, &pycodesize);
#else
	    ofd = open_tempfile();
#endif
	} else
#endif
	    ofd = stdout;

	fprintf(ofd, "#!/usr/bin/python\n");
	if (use_syslib)
	    fprintf(ofd, "import sys\n");
	else
	    fprintf(ofd, "import os\n");

	if (tapelen > 0) {
	    fprintf(ofd, "m = [0] * %d\n", tapesz);
	} else {
	    /* Dynamic arrays are 20% slower! */
	    fprintf(ofd, "from collections import defaultdict\n");
	    fprintf(ofd, "m = defaultdict(int)\n");
	}
	fprintf(ofd, "p = %d\n", tapeinit);
	break;

    case '=': I; fprintf(ofd, "m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; fprintf(ofd, "m[p] &= 255\n"); }
	I; fprintf(ofd, "v = m[p]\n");
	break;
    case 'M': I; fprintf(ofd, "m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; fprintf(ofd, "m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; fprintf(ofd, "m[p] = m[p]+v\n"); break;
    case 'Q': I; fprintf(ofd, "if (v != 0) : m[p] = %d\n", count); break;
    case 'm': I; fprintf(ofd, "if (v != 0) : m[p] = m[p]+v*%d\n", count); break;
    case 'n': I; fprintf(ofd, "if (v != 0) : m[p] = m[p]-v*%d\n", count); break;
    case 's': I; fprintf(ofd, "if (v != 0) : m[p] = m[p]+v\n"); break;

    case 'X': I; fprintf(ofd, "raise Exception('Aborting infinite loop')\n"); break;

    case '+': I; fprintf(ofd, "m[p] += %d\n", count); break;
    case '-': I; fprintf(ofd, "m[p] -= %d\n", count); break;
    case '<': I; fprintf(ofd, "p -= %d\n", count); break;
    case '>': I; fprintf(ofd, "p += %d\n", count); break;
    case '[':
	if(bytecell) { I; fprintf(ofd, "m[p] &= 255\n"); }
	I; fprintf(ofd, "while m[p] :\n"); ind++; break;
    case ']':
	if(bytecell) {
	    I; fprintf(ofd, "m[p] &= 255\n");
	}
	ind--;
	break;

    case '.':
	I; fprintf(ofd, "try:\n");
	ind++;
	if (use_syslib) {
	    I; fprintf(ofd, "sys.stdout.write(chr(m[p]))\n");
	    I; fprintf(ofd, "sys.stdout.flush()\n");
	} else {
	    I; fprintf(ofd, "os.write(1, chr(m[p]).encode('ascii'))\n");
	}
	ind--;
	I; fprintf(ofd, "except:\n");
	ind++;
	I; fprintf(ofd, "pass\n");
	ind--;
	break;

    case ',':
	if (use_syslib) {
	    I; fprintf(ofd, "c = sys.stdin.read(1);\n");
	    I; fprintf(ofd, "if c != '' :\n");
	    ind++; I; ind--; fprintf(ofd, "m[p] = ord(c)\n");
	} else {
	    I; fprintf(ofd, "c = os.read(0, 1);\n");
	    I; fprintf(ofd, "if c != '' :\n");
	    ind++; I; ind--; fprintf(ofd, "m[p] = ord(c)\n");
	}
	break;
    }

#ifndef DISABLE_LIBPY
    if (!do_dump && ch == '~') {
#ifndef DISABLE_OPENMEMSTREAM
	fclose(ofd);
#else
        reload_tempfile(ofd, &pycode, &pycodesize);
#endif

	/* The bare interpreter method. Works fine for BF code.
	 */
	Py_Initialize();
	PyRun_SimpleString(pycode);
	Py_Finalize();
    }
#endif
}
