#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if !defined(DISABLE_LIBLUA) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

#ifndef DISABLE_LIBLUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include "bf2any.h"

/*
 * Lua translation from BF, runs at about 39,000,000 instructions per second.
 * LuaJIT translation from BF, runs at about 360,000,000 instructions per second.
 *
 * There is a limit on the size of a while loop imposed by the interpreter.
 * The while loop's jump has a limited range, so if there are lots of tokens
 * in the loop I make the loop body a function.
 *
 * As I have to load the entire program into memory, I've added a variant that
 * just prints "Hello world" style programs.
 */

#define MAXWHILE 4096

struct instruction {
    int ch;
    int count;
    int ino;
    int icount;
    struct instruction * next;
    struct instruction * loop;
    char * cstr;
};
static struct instruction *pgm = 0, *last = 0, *jmpstack = 0;

static void loutcmd(int ch, int count, struct instruction *n);

static int do_input = 0, do_output = 0, do_tape = 0;
static int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")

static int lblcount = 0;
static int icount = 0;
static int maxwhile = MAXWHILE;

static void print_cstring(char * str);

static int do_dump;
static FILE * ofd;
#ifndef DISABLE_LIBLUA
static char * luacode = 0;
static size_t luacodesize = 0;
#endif

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {fn_check_arg, gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-d") == 0) {
        do_dump = 1;
        return 1;
    } else
#ifndef DISABLE_LIBLUA
    if (strcmp(arg, "-r") == 0) {
        do_dump = 0;
        return 1;
    } else
#endif
    if (strncmp(arg, "-w", 2) == 0) {
	maxwhile = strtol(arg+2, 0, 10);
	return 1;
    }
    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2lua"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (ch == '"')
	n->cstr = strdup(strn);
    else if (ch != '!' && ch != '~')
	do_tape=1;
    if (!last) {
	pgm = n;
    } else {
	last->next = n;
    }
    last = n;

    if (n->ch == '[' || n->ch == 'I') {
	n->ino = ++lblcount;
	n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']' || n->ch == 'E') {
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    }

    if (ch != '~') return;
#ifndef DISABLE_LIBLUA
    if (!do_dump) {
#ifndef DISABLE_OPENMEMSTREAM
	ofd = open_memstream(&luacode, &luacodesize);
#else
	ofd = open_tempfile();
#endif
    } else
#endif
    {
	ofd = stdout;
	fprintf(ofd, "#!/usr/bin/lua\n");   /* liblua doesn't like this */
    }

    if (do_tape) {
	fprintf(ofd, "local m = setmetatable({},{__index=function() return 0 end})\n");
	fprintf(ofd, "local p = 1\n");
	fprintf(ofd, "local v = 0\n");
	/* Arrays are faster if populated from 1, negative indexes are ok.
	   Luajit likes zero. */
	fprintf(ofd, "for i=0,32 do m[i] = 0 end\n");
	fprintf(ofd, "\n");
    }

    if (icount < maxwhile) {
	for(n=pgm; n; n=n->next)
	    loutcmd(n->ch, n->count, n);
    } else {
	for(n=pgm; n; n=n->next) {
	    if (n->ch != ']' && n->ch != 'E') continue;
	    if (n->icount-n->loop->icount <= maxwhile) continue;
	    loutcmd(1000, 1, n->loop);
	    for(n2 = n->loop->next; n != n2; n2=n2->next) {
		if ((n2->ch == '[' || n2->ch == 'I')
			&& n2->loop->icount-n2->icount > maxwhile) {
		    loutcmd(n2->ch, n2->count, n);
		    I; fprintf(ofd, "bf%d()\n", n2->ino);
		    n2 = n2->loop;
		    loutcmd(n2->ch, n2->count, n);
		} else
		    loutcmd(n2->ch, n2->count, n2);
	    }
	    loutcmd(1001, 1, n);
	}

	for(n=pgm; n; n=n->next) {
	    if ((n->ch != '[' && n->ch != 'I')
		    || n->loop->icount-n->icount <= maxwhile)
		loutcmd(n->ch, n->count, n);
	    else {
		loutcmd(n->ch, n->count, n);
		I; fprintf(ofd, "bf%d()\n", n->ino);
		n=n->loop;
		loutcmd(n->ch, n->count, n);
	    }
	}
    }

    while(pgm) {
	n = pgm;
	pgm = pgm->next;
	if (n->cstr)
	    free(n->cstr);
	memset(n, '\0', sizeof*n);
	free(n);
    }

#ifndef DISABLE_LIBLUA
    if (!do_dump && ch == '~') {
	int status, result;
	lua_State *L;
#ifndef DISABLE_OPENMEMSTREAM
        fclose(ofd);
#else
	reload_tempfile(ofd, &luacode, &luacodesize);
#endif

	L = luaL_newstate();
	luaL_openlibs(L); /* Load Lua libraries */
	status = luaL_loadstring(L, luacode);
	if (status) {
	    /* If something went wrong, error message is at the top of */
	    /* the stack */
	    fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
	    exit(1);
	}
	result = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (result) {
	    fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
	    exit(1);
	}
	lua_close(L);   /* Bye, Lua */
    }
#endif
}

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case 1000:
	fprintf(ofd, "function bf%d()\n", n->ino);
	ind++;
	break;
    case 1001:
	ind--;
	fprintf(ofd, "end\n\n");
	break;

    case '!':
	if(do_tape) {
	    fprintf(ofd, "function brainfuck()\n");
	    ind++;
	}
	break;

    case '=': I; fprintf(ofd, "m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; fprintf(ofd, "m[p] = m[p]%%256\n"); }
	I; fprintf(ofd, "v = m[p]\n");
	break;
    case 'M': I; fprintf(ofd, "m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; fprintf(ofd, "m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; fprintf(ofd, "m[p] = m[p]+v\n"); break;
    case 'T': I; fprintf(ofd, "m[p] = m[p]-v\n"); break;
    case '*': I; fprintf(ofd, "m[p] = m[p]*v\n"); break;

    case 'C': I; fprintf(ofd, "m[p] = v*%d\n", count); break;
    case 'D': I; fprintf(ofd, "m[p] = -v*%d\n", count); break;
    case 'V': I; fprintf(ofd, "m[p] = v\n"); break;
    case 'W': I; fprintf(ofd, "m[p] = -v\n"); break;

    case 'X': I; fprintf(ofd, "error('Aborting Infinite Loop')\n"); break;

    case '+': I; fprintf(ofd, "m[p] = m[p]+%d\n", count); break;
    case '-': I; fprintf(ofd, "m[p] = m[p]-%d\n", count); break;
    case '>': I; fprintf(ofd, "p = p+%d\n", count); break;
    case '<': I; fprintf(ofd, "p = p-%d\n", count); break;
    case '.': I; fprintf(ofd, "putch()\n"); do_output=1; break;
    case '"': print_cstring(n->cstr); break;
    case ',': I; fprintf(ofd, "getch()\n"); do_input=1; break;

    case '[':
	if(bytecell) { I; fprintf(ofd, "m[p] = m[p]%%256\n"); }
	I; fprintf(ofd, "while m[p]~=0 do\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; fprintf(ofd, "m[p] = m[p]%%256\n"); }
	ind--; I; fprintf(ofd, "end\n");
	break;

    case 'I':
	if(bytecell) { I; fprintf(ofd, "m[p] = m[p]%%256\n"); }
	I; fprintf(ofd, "if m[p]~=0 then\n");
	ind++;
	break;
    case 'E':
	ind--; I; fprintf(ofd, "end\n");
	break;

    case '~':
	if(do_tape) {
	    ind--;
	    fprintf(ofd, "end\n");
	}

	if (do_input) {
	    fprintf(ofd, "\n");
	    fprintf(ofd, "function getch()\n");
	    fprintf(ofd, "    local Char = io.read(1)\n");
	    fprintf(ofd, "    if Char then\n");
	    fprintf(ofd, "        m[p] = string.byte(Char)\n");
	    fprintf(ofd, "    end\n");
	    fprintf(ofd, "end\n");
	}

	if (do_output) {
	    fprintf(ofd, "\n");
	    fprintf(ofd, "function putch()\n");
	    fprintf(ofd, "    io.write(string.char(m[p]%%256))\n");
	    fprintf(ofd, "    io.flush()\n");
	    fprintf(ofd, "end\n");
	}

	if(do_tape) {
	    fprintf(ofd, "\n");
	    fprintf(ofd, "brainfuck()\n");
	}
	break;
    }
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
	    I; fprintf(ofd, "io.write(\"%s\")\n", buf);
	    gotnl = outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' ) {
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
	    sprintf(buf2, "\\%d", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
