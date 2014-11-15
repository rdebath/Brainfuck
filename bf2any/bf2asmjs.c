#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Asmjs translation From BF, performance depends on browser.
 *
 * See: https://github.com/olahol/bf2asmjs
 * And: http://asmjs.org/
 *
 * This now has the facility to get new input data after the application
 * has been started. This is done by writing some of the generated loops
 * in a coroutine form and having it return whenever it needs more data.
 * The problem with that is that it needs a Javascript goto and stupidly
 * Javascript doesn't support goto. No matter I can do the same using
 * an even more obscure but legal layout, the state machine.
 *
 * Confusion is raised to an even higher level by mixing the two
 * implementations depending on the positions of the input commands.
 *
 * See: http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
 *  (But javascript has nothing to hide the ugly details)
 *
 * This obviously needs a simple html wrapper to run it there are two in:
 *  https://github.com/rdebath/LostKingdom
 */

struct instruction {
    int ch;
    int count;
    int ino;
    int has_inp;
    struct instruction * next;
    struct instruction * loop;
} *pgm = 0, *last = 0, *jmpstack = 0;

void loutcmd(int ch, int count, struct instruction *n);

static int do_input = 0;
static int ind = 2;
static int lblcount = 0;
static int icount = 0;

static const char * mp;
static int ptrstep = 1;

#define I printf("%*s", ind*4, "")
#define IO(d) printf("%*s", (ind+(d))*4, "")

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    struct instruction * n = calloc(1, sizeof*n);
    if (!n) { perror("bf2asmjs"); exit(42); }

    icount ++;
    n->ch = ch;
    n->count = count;
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
	n->ino = ++lblcount;
	n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    } else if (ch == ',') {
	struct instruction * j = jmpstack;
	n->ino = ++lblcount;
	do_input ++;
	n->has_inp = 1;

	while(j) {
	    j->has_inp++;
	    j=j->loop;
	}
    }

    if (ch != '~') return;

    if (bytecell) {
	mp = "m[p]";
	ptrstep = 1;
    } else {
	mp = "m[p>>2]";
	ptrstep = 4;
    }

    for(n=pgm; n; n=n->next) {
	if (n->loop && n->loop->has_inp)
	    n->has_inp = n->loop->has_inp;

#ifdef COMMENTS
	if (n->ino) {
	    I; printf("// %d", n->ino);
	    if (n->loop) {
		printf(" --> %d", n->loop->ino);
	    }
	    if (n->has_inp)
		printf(" Has %d ',' command(s)", n->has_inp);
	    printf("\n");
	}
#endif

	loutcmd(n->ch, n->count, n);
    }

    while(pgm) {
	n = pgm;
	pgm = pgm->next;
	memset(n, '\0', sizeof*n);
	free(n);
    }
}

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case '=': I; printf("%s = %d;\n", mp, count); break;
    case 'B': I; printf("v = %s|0;\n", mp); break;
    case 'M': I; printf("%s = (%s|0) + (v*%d |0);\n", mp, mp, count); break;
    case 'N': I; printf("%s = (%s|0) - (v*%d |0);\n", mp, mp, count); break;
    case 'S': I; printf("%s = (%s|0) + v;\n", mp, mp); break;
    case 'Q': I; printf("if ((v|0) != 0) %s = %d;\n", mp, count); break;
    case 'm': I; printf("if ((v|0) != 0) %s = (%s|0) + (v*%d|0);\n", mp, mp, count); break;
    case 'n': I; printf("if ((v|0) != 0) %s = (%s|0) - (v*%d|0);\n", mp, mp, count); break;
    case 's': I; printf("if ((v|0) != 0) %s = (%s|0) + v;\n", mp, mp); break;

    case '+': I; printf("%s = (%s|0) + %d;\n", mp, mp, count); break;
    case '-': I; printf("%s = (%s|0) - %d;\n", mp, mp, count); break;
    case '<': I; printf("p = (p - %d)|0;\n", count*ptrstep); break;
    case '>': I; printf("p = (p + %d)|0;\n", count*ptrstep); break;
    case '.': I; printf("o(%s|0);\n", mp); break;
    }

    if (n->has_inp == 0) {
	switch(ch) {
	case '[': I; printf("while ((%s|0) != 0) {\n", mp); ind++; break;
	case ']': ind--; I; printf("}\n"); break;
	}
    } else {
	switch(ch) {
	case ',':
	    I; printf("moreinp = 1;\n");
	    I; printf("j = %d;\n", n->ino);
	    I; printf("break;\n");
	    IO(-1); printf("case %d:\n", n->ino);
	    I; printf("j = 0;\n");
	    break;
	case '[':
	    I; printf("if ((%s|0) == 0) {\n", mp);
	    IO(1); printf("j = %d;\n", n->loop->ino);
	    IO(1); printf("break;\n");
	    I; printf("}\n");
	    IO(-1); printf("case %d:\n", n->ino);
	    I; printf("j = 0;\n");
	    I; printf("//{\n");
	    ind++;
	    break;
	case ']':
	    ind--;
	    I; printf("//}\n");
	    I; printf("if ((%s|0) != 0) {\n", mp);
	    IO(1); printf("j = %d;\n", n->loop->ino);
	    IO(1); printf("break;\n");
	    I; printf("}\n");
	    IO(-1); printf("case %d:\n", n->ino);
	    I; printf("j = 0;\n");
	    break;
	}
    }

    switch(ch) {
    case '!':
	printf("%s",
		";(function (ctx) {\n"
		"function BF(stdlib, ffi, heap) {\n");
	if (icount < 10000)
	    printf("%s", "    \"use asm\";\n");
	else
	    printf("// %d instructions don't use asm\n", icount);
	printf("%s", "\n");
	if (bytecell)
	    printf("%s",
		"    var m = new stdlib.Uint8Array(heap);\n"
		);
	else
	    printf("%s",
		"    var m = new stdlib.Int32Array(heap);\n"
		);
	printf("%s%d%s",
		"    var p = ", tapeinit * (bytecell?1:4), ";\n"
		"    var v = 0;\n"
		"    var o = ffi.put;\n"
		"    var get = ffi.get;\n"
		"    var j = 0;\n"
		"    var moreinp = 0;\n"
		"\n"
		"    function run() {\n"
		);

	if (do_input)
	{
	    printf("%s",
		"        do {\n"
		);
	    ind ++;

	    I; printf("if (moreinp) {\n");
	    ind ++;
	    I; printf("v = get()|0;\n");
	    I; printf("if ((v|0) >= 0) {\n");
	    ind ++;
	    I; printf("%s = v;\n", mp);
	    I; printf("moreinp = 0;\n");
	    ind --;
	    I; printf("} else break;\n");
	    ind --;
	    I; printf("}\n");

	    printf("%s",
		"            switch(j|0)\n"
		"            {\n"
	        "            case -1: reset(); while((get()|0) != -1);\n"
		"            case 0:\n"
		"                j = 0;\n"
		);
	    ind ++;
	}
	break;

    case '~':
	if (do_input)
	{
	    I; printf("j = -1;\n");
	    ind--;
	    I; printf("};\n\n");

	    ind --;
	    I; printf("} while((j|0) > 0);\n");
	    I; printf("return j|0;\n");
	} else {
	    I; printf("return -1;\n");
	}

	printf( "    }\n"
		"\n"
		"    function reset() {\n"
		"        var i = 0;\n"
		"        while ((i|0) < (%d|0)) {\n",
		tapesz * (bytecell?1:4)
		);
	if (bytecell)
	    printf(
		"            m[i] = 0;\n"
		"            i = (i + 1)|0\n"
		);
	else
	    printf(
		"            m[i>>2] = 0;\n"
		"            i = (i + 4)|0\n"
		);
	printf(
		"        }\n"
		"        j = 0;\n"
		"        moreinp = 0;\n"
		"    }\n"
		"\n"
		"    return { reset: reset, run: run };\n"
		"}\n"
		"\n"
		"// Save BF function and interface\n"
		"    ctx[\"BFprogram\"] = BF(ctx, {\n"
		"        put: function (i) {\n"
		"                   if (i == 10) {\n"
		"                       BFoutputText += BFoutputChar + BFoutputLine;\n"
		"                       BFoutputChar = '\\n';\n"
		"                       BFoutputLine = '';\n"
		"                   } else {\n"
		"                       BFoutputLine += String.fromCharCode(i);\n"
		"                   }\n"
		"             }\n"
		"        , get: function () {\n"
		"                   var c = BFinputText.charCodeAt(BFinputPtr) || -1;\n"
		"                   BFinputPtr++;\n"
		"                   return c;\n"
		"               }\n"
		"    }, new ArrayBuffer(%d));\n"
		"})(this)\n", tapesz * (bytecell?1:4));
	break;

    }
}
