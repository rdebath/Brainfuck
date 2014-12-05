#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
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
 * LostKng.bf is unrunnable.
 */

#define MAXWHILE 256

struct instruction {
    int ch;
    int count;
    int ino;
    int icount;
    struct instruction * next;
    struct instruction * loop;
    char * cstr;
} *pgm = 0, *last = 0, *jmpstack = 0;

void loutcmd(int ch, int count, struct instruction *n);

int ind = 0;
#define I printf("%*s", ind*4, "")
static int lblcount = 0;
static int icount = 0;

static void print_cstring(char * str);
static int no_function = 0;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp(arg, "-nofunc") == 0) {
	no_function = 1;
	return 1;
    }
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-nofunc Do NOT include a function wrapper (SLOW)"
	);
	return 1;
    }

    return 0;
}

void
outcmd(int ch, int count)
{
    struct instruction * n = calloc(1, sizeof*n), *n2;
    if (!n) { perror("bf2any"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
    n->icount = icount;
    if (ch == '"')
	n->cstr = strdup(get_string());
    if (!last) {
	pgm = n;
    } else {
	last->next = n;
    }
    last = n;

    if (n->ch == '[') {
	n->ino = ++lblcount;
	n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']') {
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    }

    if (ch != '~') return;

    if (icount == 3 && pgm->ch == '!' && pgm->next->ch == '"' &&
	    pgm->next->next->ch == '~') {
	loutcmd('"', 0, pgm->next);
    } else {

	loutcmd(999, 1, n);

	if (icount < MAXWHILE || no_function) {
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

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case 999:
	if (bytecell)
	    printf( "%s%d%s%d%s",
		    "m = zeros(Uint8, ",tapesz,")\n"
		    "p = ", tapeinit, "\n");
	else
	    printf( "%s%d%s%d%s",
		    "m = zeros(typeof(0), ",tapesz,")\n"
		    "p = ", tapeinit, "\n");
	break;
    case 1000:
	printf("function bf%d(m,p)\n", n->ino);
	ind++;
	break;
    case 1001:
	ind--;
	printf("end\n\n");
	break;
    case 1002:
	I; printf("bf%d(m,p)\n", count);
	break;

    case '!':
	if (!no_function) puts("function brainfuck(m,p)");
	break;
    case '~':
	if (!no_function) puts("end\nbrainfuck(m,p)");
	break;

    case '=': I; printf("m[p] = %d\n", count); break;
    case 'B':
	I; printf("v = m[p]\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v\n"); break;
    case 'Q': I; printf("if v != 0 ; m[p] = %d; end\n", count); break;
    case 'm': I; printf("if v != 0 ; m[p] = m[p]+v*%d; end\n", count); break;
    case 'n': I; printf("if v != 0 ; m[p] = m[p]-v*%d; end\n", count); break;
    case 's': I; printf("if v != 0 ; m[p] = m[p]+v; end\n"); break;

    case 'X': I; printf("error(\"Aborting Infinite Loop.\")\n"); break;

    case '+': I; printf("m[p] += %d\n", count); break;
    case '-': I; printf("m[p] -= %d\n", count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	I; printf("while m[p] != 0\n");
	ind++;
	break;
    case ']':
	ind--; I; printf("end\n");
	break;
    case '.': I; printf("print(char(m[p]))\n"); break;
    case '"': print_cstring(n->cstr); break;
    case ',': I; printf("m[p] = read(STDIN, Char)\n"); break;
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
