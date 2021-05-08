#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if defined(ENABLE_LIBLUA) && (_POSIX_VERSION < 200809L)
#include "usemktemp.c"
#endif

#ifdef ENABLE_LIBLUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include "bf2any.h"
#include "move_opt.h"

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

#define MAXJUMP 65000
#define MAXDEPTH 192

struct instruction {
    int ch;
    int count;
    int mov;
    int ino;
    int icount;
    int nest_count;
    int use_function;
    struct instruction * next;
    struct instruction * loop;
    struct instruction * up;
    char * cstr;
};
static struct instruction *pgm = 0, *last = 0, *jmpstack = 0;

static void loutcmd(int ch, int count, int mov, struct instruction *n);

static int do_input = 0, do_output = 0, do_tape = 0;
static int ind = 0;
#define I fprintf(ofd, "%*s", ind*4, "")

static int lblcount = 0;
static int icount = 0;
static int maxjump = MAXJUMP;
static int maxdepth = MAXDEPTH;
static int nest_count = 0;

static void print_cstring(char * str);

static int do_dump;
static int do_dbl;
static FILE * ofd;
#ifdef ENABLE_LIBLUA
static char * luacode = 0;
static size_t luacodesize = 0;
#endif

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
#ifdef ENABLE_LIBLUA
    if (strcmp(arg, "-r") == 0) {
        do_dump = 0;
        return 1;
    } else
#endif
    if (strcmp(arg, "-dbl") == 0) {
        do_dbl = 1;
        return 1;
    } else
    if (strncmp(arg, "-J", 2) == 0) {
	maxjump = strtol(arg+2, 0, 10);
	return 1;
    }
    if (strncmp(arg, "-D", 2) == 0) {
	maxdepth = strtol(arg+2, 0, 10);
	return 1;
    }
    return 0;
}

static const char *
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

static void
gen_code(int ch, int count, char * strn)
{
    int mov = 0;
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2lua"); exit(42); }

    move_opt(&ch, &count, &mov);
    if (ch == 0) return;

    /* Estimate the lengths a little better than "everything is 16" */
    switch(ch)
    {
    case '.': case ',': icount += 4; break;
    case '>': case '<': icount += 6; break;
    case '=': case 'V': icount += 8; break;
    case '+': case '-': icount += 14; break;
    case 'M': case 'N': icount += 18; break;
    case 'B': case 'I': case '[': case ']':
	icount += 8 + 16*(bytecell!=0);
	break;
    default: icount += 16; break;
    }
    if (mov) icount += 2;

    n->ch = ch;
    n->count = count;
    n->mov = mov;
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
	if (jmpstack) {
	    if (jmpstack->nest_count <= n->loop->nest_count)
		jmpstack->nest_count = n->loop->nest_count+1;
	} else if (nest_count <= n->loop->nest_count)
	    nest_count = n->loop->nest_count+1;
	n->loop->up = jmpstack;
    }

    if (ch != '~') return;
#ifdef ENABLE_LIBLUA
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
	fprintf(ofd, "%s\n",
	    "if jit then jit.opt.start(\"loopunroll=200\") end");

	if (do_dbl)
	    fprintf(ofd, "%s\n",
		     "local m = setmetatable({},{__index=function() return 0 end})"
		"\n" "local v = 0"
		"\n" "local p = 1"
		"\n" "local div = function(n,d) return math.floor(n/d); end"
		"\n" "do local i; for i=1,32 do m[i] = 0 end; end"
		);
	else {
	    const char * luajit_type = bytecell? "uint8_t": "uint64_t";
	    /* For Lua Arrays are faster if populated from 1, negative
	     * indexes are ok. Luajit likes zero. Luajit's ffi gives me a
	     * real 64bit unsigned int, so divmod can work properly. The
	     * 32bit ints do NOT work, but uint8_t does and is rather quick.
	     */
	    fprintf(ofd, "%s%s%s%d%s%s%s%d%s\n",
		     "local m, v, p, ffi, div"
		"\n" "if type(rawget(_G, \"jit\")) ~= 'table' then"
		"\n" "  m = setmetatable({},{__index=function() return 0 end})"
		"\n" "  v = 0"
		"\n" "  p = 1"
		"\n" "  local i"
		"\n" "  for i=1,32 do m[i] = 0 end"
		"\n" "  div = function(n,d) return math.floor(n/d); end"
		"\n" "else"
		"\n" "  ffi = require(\"ffi\")"
		"\n" "  m = ffi.new(\"",luajit_type,"[",tapesz,"]\");"
		"\n" "  v = ffi.new(\"",luajit_type,"\")"
		"\n" "  p = ",tapeinit,""
		"\n" "  div = function(n,d) return n/d; end"
		"\n" "end"
		);
	}
    }

    if (icount < maxjump && nest_count < maxdepth) {
	for(n=pgm; n; n=n->next)
	    loutcmd(n->ch, n->count, n->mov, n);
    } else {
	for(n=pgm; n; n=n->next) {
	    int idiff;
	    if (n->ch != ']' && n->ch != 'E') continue;
	    idiff = n->icount-n->loop->icount;
	    if (idiff <= maxjump && n->loop->nest_count < maxdepth) continue;
	    loutcmd(1000, 1, 0, n->loop);
	    for(n2 = n->loop->next; n != n2; n2=n2->next) {
		if ((n2->ch == '[' || n2->ch == 'I') && n2->use_function) {
		    loutcmd(n2->ch, n2->count, n2->mov, n2);
		    I; fprintf(ofd, "bf%d()\n", n2->ino);
		    n2 = n2->loop;
		    loutcmd(n2->ch, n2->count, n2->mov, n2);
		} else
		    loutcmd(n2->ch, n2->count, n2->mov, n2);
	    }
	    loutcmd(1001, 1, 0, n);

	    n->loop->use_function = 1;

	    /* Reduce the later icounts so I don't keep creating functions */
	    for(n2=n->next; n2; n2=n2->next)
		n2->icount -= idiff;

	    /* Recalculate how many nested loops too */
	    for(n2=pgm; n2; n2=n2->next) {
		if (n2->ch == '[' || n2->ch == 'I')
		    n2->nest_count = 0;
		else if (n2->ch == ']' || n2->ch == 'E') {
		    struct instruction * n3 = n2->loop, *n4 = n3->up;

		    if (n4 && !n4->use_function && n3->nest_count >= n4->nest_count)
			n4->nest_count = n3->nest_count+1;
		}
	    }
	}

	for(n=pgm; n; n=n->next) {
	    if ((n->ch != '[' && n->ch != 'I') || n->use_function == 0)
		loutcmd(n->ch, n->count, n->mov, n);
	    else {
		loutcmd(n->ch, n->count, n->mov, n);
		I; fprintf(ofd, "bf%d()\n", n->ino);
		n=n->loop;
		loutcmd(n->ch, n->count, n->mov, n);
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

#ifdef ENABLE_LIBLUA
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
loutcmd(int ch, int count, int mov, struct instruction *n)
{
    const char *cm = cell(mov);

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

    case '=': I; fprintf(ofd, "%s = %d\n", cm, count); break;
    case 'B':
	if(bytecell) { I; fprintf(ofd, "%s = %s%%256\n", cm, cm); }
	I; fprintf(ofd, "v = %s\n", cm);
	break;
    case 'M': I; fprintf(ofd, "%s = %s+v*%d\n", cm, cm, count); break;
    case 'N': I; fprintf(ofd, "%s = %s-v*%d\n", cm, cm, count); break;
    case 'S': I; fprintf(ofd, "%s = %s+v\n", cm, cm); break;
    case 'T': I; fprintf(ofd, "%s = %s-v\n", cm, cm); break;
    case '*': I; fprintf(ofd, "%s = %s*v\n", cm, cm); break;
    case '/': I; fprintf(ofd, "%s = div(%s, v)\n", cm, cm); break;
    case '%': I; fprintf(ofd, "%s = %s%%v\n", cm, cm); break;

    case 'C': I; fprintf(ofd, "%s = v*%d\n", cm, count); break;
    case 'D': I; fprintf(ofd, "%s = -v*%d\n", cm, count); break;
    case 'V': I; fprintf(ofd, "%s = v\n", cm); break;
    case 'W': I; fprintf(ofd, "%s = -v\n", cm); break;

    case 'X': I; fprintf(ofd, "error('Aborting Infinite Loop')\n"); break;

    case '+': I; fprintf(ofd, "%s = %s+%d\n", cm, cm, count); break;
    case '-': I; fprintf(ofd, "%s = %s-%d\n", cm, cm, count); break;
    case '>': I; fprintf(ofd, "p = p+%d\n", count); break;
    case '<': I; fprintf(ofd, "p = p-%d\n", count); break;
    case '.': I; fprintf(ofd, "putch(%d)\n", mov); do_output=1; break;
    case '"': print_cstring(n->cstr); break;
    case ',': I; fprintf(ofd, "getch(%d)\n", mov); do_input=1; break;

    case '[':
	if(bytecell) { I; fprintf(ofd, "%s = %s%%256\n", cm, cm); }
	I; fprintf(ofd, "while %s~=0 do\n", cm);
	ind++;
	break;
    case ']':
        if (count > 0) {
            I; fprintf(ofd, "p = p + %d\n", count);
        } else if (count < 0) {
            I; fprintf(ofd, "p = p - %d\n", -count);
        }

	if(bytecell) { I; fprintf(ofd, "%s = %s%%256\n", cm, cm); }
	ind--; I; fprintf(ofd, "end\n");
	break;

    case 'I':
	if(bytecell) { I; fprintf(ofd, "%s = %s%%256\n", cm, cm); }
	I; fprintf(ofd, "if %s~=0 then\n", cm);
	ind++;
	break;
    case 'E':
        if (count > 0) {
            I; fprintf(ofd, "p = p + %d\n", count);
        } else if (count < 0) {
            I; fprintf(ofd, "p = p - %d\n", -count);
        }

	ind--; I; fprintf(ofd, "end\n");
	break;

    case '~':
	if(do_tape) {
	    ind--;
	    fprintf(ofd, "end\n");
	}

	if (do_input) {
	    fprintf(ofd, "\n");
	    fprintf(ofd, "function getch(poff)\n");
	    fprintf(ofd, "    local Char = io.read(1)\n");
	    fprintf(ofd, "    if Char then\n");
	    fprintf(ofd, "        m[p+poff] = string.byte(Char)\n");
	    fprintf(ofd, "    end\n");
	    fprintf(ofd, "end\n");
	}

	if (do_output) {
	    fprintf(ofd, "\n");
	    fprintf(ofd, "function putch(poff)\n");
	    fprintf(ofd, "    io.write(string.char(tonumber(m[p+poff]%%256)))\n");
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
