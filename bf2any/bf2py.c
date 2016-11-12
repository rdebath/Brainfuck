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
int use_oslib = 0;
int use_putcell = 0;
int use_getcell = 0;

FILE * ofd;
char * pycode = 0;
size_t pycodesize = 0;

int disable_savestring = 1;

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {fn_check_arg};

static int
fn_check_arg(const char * arg)
{
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
	use_oslib = 1;
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
	fprintf(ofd, "import sys\n");

	if (use_oslib)
	    fprintf(ofd, "import os,platform\n");

	if (tapelen <= 0)
	    fprintf(ofd, "from collections import defaultdict\n");

	fprintf(ofd, "\ndef brainfuck(argv):\n");
	ind++;

	if (tapelen > 0) {
	    I; fprintf(ofd, "m = [0] * %d\n", tapesz);
	} else {
	    /* Dynamic arrays are 20% slower! */
	    I; fprintf(ofd, "m = defaultdict(int)\n");
	}
	I; fprintf(ofd, "p = %d\n", tapeinit);
	break;

    case '~':
	I; fprintf(ofd, "return 0\n\n");
	ind--;

	if (use_putcell) {
	    if (use_oslib) {
		I; fprintf(ofd, "if (platform.python_implementation() == 'PyPy'):\n");
		ind++;
		I; fprintf(ofd, "def putcell(v):\n");
		ind++;
		I; fprintf(ofd, "os.write(1, chr(v %% 255))\n");
		ind--;
		ind--;
		I; fprintf(ofd, "else:\n"); ind++;
	    }

	    I; fprintf(ofd, "def putcell(v):\n");
	    ind++;
	    I; fprintf(ofd, "try:\n");
	    ind++;
	    if (!use_oslib) {
		I; fprintf(ofd, "sys.stdout.write(chr(v))\n");
		I; fprintf(ofd, "sys.stdout.flush()\n");
	    } else {
		I; fprintf(ofd, "os.write(1, chr(v).encode('ascii'))\n");
	    }
	    ind--;
	    I; fprintf(ofd, "except:\n");
	    ind++;
	    I; fprintf(ofd, "pass\n");
	    ind--;
	    ind--;
	    if (use_oslib) ind--;
	    fprintf(ofd, "\n");
	}

	if (use_getcell) {
	    I; fprintf(ofd, "if (platform.python_implementation() == 'PyPy'):\n");
	    ind++;
	    I; fprintf(ofd, "def getcell(v):\n");
	    ind++;
	    I; fprintf(ofd, "return ord(os.read(0, 1)[0])\n");
	    ind--;
	    ind--;
	    I; fprintf(ofd, "else:\n");
	    ind++;
	    I; fprintf(ofd, "def getcell(v):\n");
	    ind++;
	    I; fprintf(ofd, "try:\n");
	    ind++;

	    I; fprintf(ofd, "c = os.read(0, 1);\n");
	    I; fprintf(ofd, "if c != '' :\n");
	    ind++;
	    I; fprintf(ofd, "v = ord(c)\n");
	    ind--;
	    ind--;
	    I; fprintf(ofd, "except:\n");
	    ind++;
	    I; fprintf(ofd, "pass\n");
	    ind--;
	    I; fprintf(ofd, "return v\n");

	    ind--;
	    ind--;
	    fprintf(ofd, "\n");
	}

	if (!do_dump) {
	    I; fprintf(ofd, "brainfuck(None)\n");
	    break;
	}

	I; fprintf(ofd, "if __name__ == \"__main__\":\n"); ind++;
	I; fprintf(ofd, "brainfuck(sys.argv)\n"); ind--;

	if (use_oslib && tapelen>0) {
	    fprintf(ofd, "\n");
	    I; fprintf(ofd, "def target(*args):\n"); ind++;
	    I; fprintf(ofd, "return brainfuck, None\n"); ind--;
	}
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
	I; fprintf(ofd, "putcell(m[p])\n");
	use_putcell = 1;
	break;

    case ',':
	if (!use_oslib) {
	    I; fprintf(ofd, "c = sys.stdin.read(1);\n");
	    I; fprintf(ofd, "if c != '' :\n");
	    ind++; I; ind--; fprintf(ofd, "m[p] = ord(c)\n");
	} else {
	    I; fprintf(ofd, "m[p] = getcell(m[p])\n");
	    use_getcell = 1;
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
