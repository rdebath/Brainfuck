#ifndef DISABLE_LIBPY
/* This must be first; hopefully nothing else "must be first" too */
#include <Python.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

/* Ah, yes, this should be first too ... idiots. */
#if !defined(DISABLE_LIBPY) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

/*
 * Python translation from BF, runs at about 18,000,000 instructions per second.
 *
 * There is a limit on the number of nested loops was 20 now 100.
 */

static int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")

static int do_dump = 0;
static int use_oslib = 0;
static int use_putcell = 0;
static int use_putstr = 0;
static int use_getcell = 0;

static FILE * ofd;
#ifndef DISABLE_LIBPY
static char * pycode = 0;
static size_t pycodesize = 0;
#endif

static void print_string(void);

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {.check_arg=fn_check_arg};

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

static char *
cell(int mov)
{
    static char buf[6+3+sizeof(mov)*3];
    if (mov == 0) return "m[p]";
    if (mov < 0)
	sprintf(buf, "m[p-%d]", -mov);
    else
	sprintf(buf, "m[p+%d]", mov);
    return buf;
}

void
outcmd(int ch, int count)
{
    int mov = 0;
    char * mc;

    move_opt(&ch, &count, &mov);
    if (ch == 0) return;

    mc = cell(mov);

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

	if (use_oslib) {
	    I; fprintf(ofd, "if __name__ == \"__main__\":\n"); ind++;
	}

	if (use_putcell && !use_oslib) {

	    I; fprintf(ofd, "try:\n");
	    I; fprintf(ofd, "    unichr\n");
	    I; fprintf(ofd, "except NameError:\n");
	    ind++;
	    I; fprintf(ofd, "def putcell(v):\n");
	    if(bytecell) {
		I; fprintf(ofd, "    v &= 255\n");
	    }
	    I; fprintf(ofd, "    try:\n");
	    I; fprintf(ofd, "        sys.stdout.write(chr(v))\n");
	    I; fprintf(ofd, "        sys.stdout.flush()\n");
	    I; fprintf(ofd, "    except ValueError:\n");
	    I; fprintf(ofd, "        try:\n");
	    I; fprintf(ofd, "            sys.stdout.write(chr(0xFFFD))\n");
	    I; fprintf(ofd, "        except ValueError:\n");
	    I; fprintf(ofd, "            sys.stdout.write('?')\n");

	    ind--;
	    I; fprintf(ofd, "else:\n");
	    ind++;

	    I; fprintf(ofd, "def putcell(v):\n");
	    if(bytecell) {
		I; fprintf(ofd, "    sys.stdout.write(chr(v & 255))\n");
		I; fprintf(ofd, "    sys.stdout.flush()\n");
	    } else {
		I; fprintf(ofd, "    try:\n");
		I; fprintf(ofd, "        sys.stdout.write(unichr(v))\n");
		I; fprintf(ofd, "        sys.stdout.flush()\n");
		I; fprintf(ofd, "    except ValueError:\n");
		I; fprintf(ofd, "        sys.stdout.write(chr(v & 255))\n");
	    }

	    ind--;
	    fprintf(ofd, "\n");
	}

	if (use_putcell && use_oslib) {

	    I; fprintf(ofd, "try:\n");
	    I; fprintf(ofd, "    unichr\n");
	    I; fprintf(ofd, "except NameError:\n");
	    ind++;

	    I; fprintf(ofd, "def putcell(v):\n");
	    I; fprintf(ofd, "    try:\n");
	    I; fprintf(ofd, "        os.write(1, chr(v).encode())\n");
	    I; fprintf(ofd, "    except ValueError:\n");
	    I; fprintf(ofd, "        pass\n");

	    ind--;
	    I; fprintf(ofd, "else:\n");
	    ind++;

	    if(bytecell) {
		I; fprintf(ofd, "def putcell(v):\n");
		I; fprintf(ofd, "    os.write(1, chr(v & 255))\n");
	    } else {
		I; fprintf(ofd, "def putcell(v):\n");
		I; fprintf(ofd, "    try:\n");
		I; fprintf(ofd, "        os.write(1, unichr(v).encode('utf-8'))\n");
		I; fprintf(ofd, "    except ValueError:\n");
		I; fprintf(ofd, "        pass\n");
	    }

	    ind--;
	    fprintf(ofd, "\n");
	}

	if (use_putstr) {
	    I; fprintf(ofd, "def putstr(v):\n");
	    I; fprintf(ofd, "    os.write(1, v.encode('ascii'))\n");
	    fprintf(ofd, "\n");
	}

	if (use_getcell) {
	    I; fprintf(ofd, "def getcell(v):\n");
	    I; fprintf(ofd, "    try:\n");
	    I; fprintf(ofd, "        c = os.read(0, 1);\n");
	    I; fprintf(ofd, "        if c != '' :\n");
	    I; fprintf(ofd, "            v = ord(c)\n");
	    I; fprintf(ofd, "    except ValueError:\n");
	    I; fprintf(ofd, "        pass\n");
	    I; fprintf(ofd, "    return v\n");
	    fprintf(ofd, "\n");
	}

	if (!do_dump) {
	    I; fprintf(ofd, "brainfuck(None)\n");
	    ind--;
	    break;
	}

	if (!use_oslib) {
	    I; fprintf(ofd, "if __name__ == \"__main__\":\n"); ind++;
	}

	I; fprintf(ofd, "brainfuck(sys.argv)\n");
	ind--;

	if (use_oslib && tapelen>0) {
	    fprintf(ofd, "\n");
	    I; fprintf(ofd, "if __name__ == \"__rpython__\":\n"); ind++;
	    ind++;
	    I; fprintf(ofd, "def target(*args):\n");
	    I; fprintf(ofd, "    return brainfuck, None\n");

	    if (use_putcell) {
		I; fprintf(ofd, "def putcell(v):\n");
		I; fprintf(ofd, "    os.write(1, chr(v & 255))\n");
	    }
	    if (use_putstr) {
		I; fprintf(ofd, "def putstr(v):\n");
		I; fprintf(ofd, "    os.write(1, v)\n");
	    }
	    if (use_getcell) {
		I; fprintf(ofd, "def getcell(v):\n");
		I; fprintf(ofd, "    return ord(os.read(0, 1)[0])\n");
	    }
	    ind--;
	}
	break;

    case '=': I; fprintf(ofd, "%s = %d\n", mc, count); break;
    case 'B':
	if(bytecell) { I; fprintf(ofd, "%s &= 255\n", mc); }
	I; fprintf(ofd, "v = %s\n", mc);
	break;
    case 'M': I; fprintf(ofd, "%s = %s+v*%d\n", mc, mc, count); break;
    case 'N': I; fprintf(ofd, "%s = %s-v*%d\n", mc, mc, count); break;
    case 'S': I; fprintf(ofd, "%s = %s+v\n", mc, mc); break;
    case 'T': I; fprintf(ofd, "%s = %s-v\n", mc, mc); break;
    case '*': I; fprintf(ofd, "%s = %s*v\n", mc, mc); break;

    case 'C': I; fprintf(ofd, "%s = v*%d\n", mc, count); break;
    case 'D': I; fprintf(ofd, "%s = -v*%d\n", mc, count); break;
    case 'V': I; fprintf(ofd, "%s = v\n", mc); break;
    case 'W': I; fprintf(ofd, "%s = -v\n", mc); break;

    case 'X': I; fprintf(ofd, "raise Exception('Aborting infinite loop')\n"); break;

    case '+': I; fprintf(ofd, "%s += %d\n", mc, count); break;
    case '-': I; fprintf(ofd, "%s -= %d\n", mc, count); break;
    case '<': I; fprintf(ofd, "p -= %d\n", count); break;
    case '>': I; fprintf(ofd, "p += %d\n", count); break;
    case '[':
	if(bytecell) { I; fprintf(ofd, "%s &= 255\n", mc); }
	I; fprintf(ofd, "while %s :\n", mc); ind++; break;
    case ']':
        if (count > 0) {
            I; fprintf(ofd, "p += %d\n", count);
        } else if (count < 0) {
            I; fprintf(ofd, "p -= %d\n", -count);
	}

	if(bytecell) {
	    I; fprintf(ofd, "%s &= 255\n", mc);
	}
	ind--;
	break;

    case 'I':
	if(bytecell) { I; fprintf(ofd, "%s &= 255\n", mc); }
	I; fprintf(ofd, "if %s :\n", mc); ind++; break;
    case 'E':
        if (count > 0) {
            I; fprintf(ofd, "p += %d\n", count);
        } else if (count < 0) {
            I; fprintf(ofd, "p -= %d\n", -count);
	}
	ind--;
	break;

    case '"': print_string(); break;
    case '.':
	I; fprintf(ofd, "putcell(%s)\n", mc);
	use_putcell = 1;
	break;

    case ',':
	if (!use_oslib) {
	    I; fprintf(ofd, "sys.stdout.flush()\n");
	    I; fprintf(ofd, "c = sys.stdin.read(1);\n");
	    I; fprintf(ofd, "if c != '' :\n");
	    ind++; I; fprintf(ofd, "%s = ord(c)\n", mc); ind--;
	} else {
	    I; fprintf(ofd, "%s = getcell(%s)\n", mc, mc);
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

static void
print_string()
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

	    if (use_oslib) {
		I; fprintf(ofd, "putstr('%s')\n", buf);
		use_putstr = 1;
	    } else if (gotnl) {
		buf[outlen-2] = 0;
		I; fprintf(ofd, "print('%s')\n", buf);
	    } else {
		I; fprintf(ofd, "sys.stdout.write('%s')\n", buf);
	    }

	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '\'' || *str == '\\') {
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
