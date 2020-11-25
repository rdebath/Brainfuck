#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bf2any.h"

/*
 * Julia translation from BF, runs at about 2,000,000,000 instructions per second.
 * Julia translation from BF, runs at about 20,000,000 instructions per second.
 *
 * But is VERY variable presumably due to the dynamic JIT.
 * Without the function it only manages 20,000,000 instructions per second.
 *
 * This is because Julia only properly compiles functions, however,
 * it takes a very long time to compile large functions and programs.
 *
 * LostKng.bf can be run if optimisation or compilation is disabled completely.
 */

#define MAXWHILE 50

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

static int ind = 0;
#define I printf("%*s", ind*4, "")
static int lblcount = 0;
static int icount = 0;
static char * inttype = "Int";

static void print_cstring(char * str);
static int use_function = -1;

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {fn_check_arg, gen_code, .cells_are_ints=1};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-nofunc") == 0) {
	use_function = 0;
	return 1;
    }
    if (strcmp(arg, "-func") == 0) {
	use_function = 1;
	return 1;
    }

    if (strcmp(arg, "-b32") == 0) { inttype = "Int32"; return 1; }
    if (strcmp(arg, "-b64") == 0) { inttype = "Int64"; return 1; }
    if (strcmp(arg, "-b128") == 0) { inttype = "Int128"; return 1; }

    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-nofunc Do NOT include a function wrapper (SLOW)"
	"\n\t"    "-b32    Use Int32"
	"\n\t"    "-b64    Use Int64"
	"\n\t"    "-b128   Use Int128"
	);
	return 1;
    }

    return 0;
}

static void
gen_code(int ch, int count, char * strn)
{
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2any"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (ch == '"')
	n->cstr = strdup(strn);
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

    if (icount == 3 && use_function != 1 && pgm->ch == '!' &&
	    pgm->next->ch == '"' && pgm->next->next->ch == '~') {
	loutcmd('"', 0, pgm->next);
    } else {

	if (icount < MAXWHILE/4 && use_function < 0)
	    use_function = 0;

	if (!use_function) {
	    for(n=pgm; n; n=n->next)
		loutcmd(n->ch, n->count, n);
	} else {
	    for(n=pgm; n; n=n->next) {
		if (n->ch != ']') continue;
		if (n->icount-n->loop->icount <= MAXWHILE) continue;
		loutcmd(1000, 1, n->loop);
		for(n2 = n->loop->next; n != n2; n2=n2->next) {
		    if (n2->ch == '[' && n2->loop->icount-n2->icount > MAXWHILE) {
			loutcmd(n2->ch, n2->count, n);
			loutcmd(1002, n2->ino, n);
			n2 = n2->loop;
			loutcmd(n2->ch, n2->count, n);
		    } else
			loutcmd(n2->ch, n2->count, n2);
		}
		loutcmd(1001, 1, n);
	    }

	    for(n=pgm; n; n=n->next) {
		if (n->ch != '[' || n->loop->icount-n->icount <= MAXWHILE)
		    loutcmd(n->ch, n->count, n);
		else {
		    loutcmd(n->ch, n->count, n);
		    loutcmd(1002, n->ino, n);
		    n=n->loop;
		    loutcmd(n->ch, n->count, n);
		}
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
}

static void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case 999:
	break;
    case 1000:
	I; printf("function bf%d(m,p)\n", n->ino);
	ind++;
	break;
    case 1001:
	I; printf("p\n");
	ind--;
	I; printf("end\n\n");
	break;
    case 1002:
	I; printf("p = bf%d(m,p)\n", count);
	break;

    case '!':
	if (use_function)
	    puts("function brainfuck(m,p)");
	else
	    printf( "%s%s%s%d%s%d%s",
		    "let m = zeros(", inttype, ", ",tapesz,"), "
		    "p = ", tapeinit, "\n");
	ind++;
	break;
    case '~':
	if (use_function)
	{
	    I; puts("p");
	    ind--;
	    I; puts("end\n");
	    I; printf("brainfuck(zeros(%s,%d), %d)\n", inttype, tapesz, tapeinit);
	} else {
	    ind--;
	    I; puts("end");
	}
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	if(bytecell) { I; printf("m[p] = mod(m[p], 256);\n"); }
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'T': I; printf("m[p] = m[p]-v\n"); break;
    case '*': I; printf("m[p] = m[p]*v\n"); break;

    case '/': I; printf("m[p] = signed(div(unsigned(m[p]), unsigned(v)))\n"); break;
    case '%': I; printf("m[p] = signed(mod(unsigned(m[p]), unsigned(v)))\n"); break;

    case 'C': I; printf("m[p] = v*%d\n", count); break;
    case 'D': I; printf("m[p] = -v*%d\n", count); break;
    case 'V': I; printf("m[p] = v\n"); break;
    case 'W': I; printf("m[p] = -v\n"); break;

    case 'X': I; printf("error(\"Aborting Infinite Loop.\")\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("m[p] = mod(m[p], 256);\n"); }
	I; printf("while m[p] != 0\n");
	ind++;
	break;
    case ']':
	if(bytecell) { I; printf("m[p] = mod(m[p], 256);\n"); }
	ind--; I; printf("end\n");
	break;
    case 'I':
	if(bytecell) { I; printf("m[p] = mod(m[p], 256);\n"); }
	I; printf("if m[p] != 0\n");
	ind++;
	break;
    case 'E':
	ind--; I; printf("end\n");
	break;
    case '.':
	if (bytecell) {
	    I; printf("m[p] = ( m[p] + 256 ) %% 256;\n");
	    I; printf("print(Char(m[p]))\n");
	} else {
	    I; printf("if m[p] >= 0 && m[p] < 200000\n");
	    I; printf("    print(Char(m[p]))\n");
	    I; printf("else\n");
	    I; printf("    print(Char(0xFFFD))\n");
	    I; printf("end\n");
	}
	break;
    case '"': print_cstring(n->cstr); break;
    case ',': I; printf("if !eof(stdin) ; m[p] = read(stdin, Char) ; end\n"); break;
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
	    if (gotnl) {
		buf[outlen-2] = 0;
		I; printf("println(\"%s\")\n", buf);
	    } else {
		I; printf("print(\"%s\")\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '$') {
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
