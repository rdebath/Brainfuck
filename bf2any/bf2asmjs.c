#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * Asmjs translation From BF, runs at about ??? instructions per second.
 *
 * See: https://github.com/olahol/bf2asmjs
 * And: http://asmjs.org/
 */

extern int bytecell;
#define MEMSIZE 65536

int do_input = 0;
int ind = 3;
#define I printf("%*s", ind*4, "")

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    if (bytecell) {
	switch(ch) {

	case '=': I; printf("m[p] = %d\n", count); break;
	case 'B': I; printf("v = m[p]|0\n"); break;
	case 'M': I; printf("m[p] = (m[p]|0) + (v*%d |0);\n", count); break;
	case 'N': I; printf("m[p] = (m[p]|0) - (v*%d |0);\n", count); break;
	case 'S': I; printf("m[p] = (m[p]|0) + v\n"); break;
	case 'Q': I; printf("if ((v|0) != 0) m[p] = %d;\n", count); break;
	case 'm': I; printf("if ((v|0) != 0) m[p] = (m[p]|0) + (v*%d|0);\n", count); break;
	case 'n': I; printf("if ((v|0) != 0) m[p] = (m[p]|0) - (v*%d|0);\n", count); break;
	case 's': I; printf("if ((v|0) != 0) m[p] = (m[p]|0) + v;\n"); break;

	case '+': I; printf("m[p] = (m[p]|0) + %d;\n", count); break;
	case '-': I; printf("m[p] = (m[p]|0) - %d;\n", count); break;
	case '<': I; printf("p = (p - %d)|0;\n", count); break;
	case '>': I; printf("p = (p + %d)|0;\n", count); break;
	case '[': I; printf("while ((m[p]|0) != 0) {\n"); ind++; break;
	case ']': ind--; I; printf("};\n"); break;
	case '.': I; printf("put(m[p]|0);\n"); break;
	case ',': I; printf("m[p] = get()|0;\n"); break;
	}

    } else {

	switch(ch) {

	case '=': I; printf("m[p>>2] = %d\n", count); break;
	case 'B': I; printf("v = m[p>>2]|0\n"); break;
	case 'M': I; printf("m[p>>2] = (m[p>>2]|0) + (v*%d |0);\n", count); break;
	case 'N': I; printf("m[p>>2] = (m[p>>2]|0) - (v*%d |0);\n", count); break;
	case 'S': I; printf("m[p>>2] = (m[p>>2]|0) + v\n"); break;
	case 'Q': I; printf("if ((v|0) != 0) m[p>>2] = %d;\n", count); break;
	case 'm': I; printf("if ((v|0) != 0) m[p>>2] = (m[p>>2]|0) + (v*%d|0);\n", count); break;
	case 'n': I; printf("if ((v|0) != 0) m[p>>2] = (m[p>>2]|0) - (v*%d|0);\n", count); break;
	case 's': I; printf("if ((v|0) != 0) m[p>>2] = (m[p>>2]|0) + v;\n"); break;

	case '+': I; printf("m[p>>2] = (m[p>>2]|0) + %d;\n", count); break;
	case '-': I; printf("m[p>>2] = (m[p>>2]|0) - %d;\n", count); break;
	case '<': I; printf("p = (p - %d)|0;\n", count*4); break;
	case '>': I; printf("p = (p + %d)|0;\n", count*4); break;
	case '[': I; printf("while ((m[p>>2]|0) != 0) {\n"); ind++; break;
	case ']': ind--; I; printf("};\n"); break;
	case '.': I; printf("put(m[p>>2]|0);\n"); break;
	case ',': I; printf("m[p>>2] = get()|0;\n"); break;
	}
    }

    switch(ch) {
    case '!':
	printf("%s",
		";(function (ctx) {\n"
		"    function BF(stdlib, ffi, heap) {\n"
		"        \"use asm\";\n"
		"\n"
		);
	if (bytecell)
	    printf("%s",
		"        var m = new stdlib.Uint8Array(heap);\n"
		);
	else
	    printf("%s",
		"        var m = new stdlib.Int32Array(heap);\n"
		);
	printf("%s%d%s",
		"        var p = ", BOFF * (bytecell?1:4), ";\n"
		"        var v = 0;\n"
		"        var put = ffi.put;\n"
		"        var get = ffi.get;\n"
		"\n"
		"        function run() {\n"
		);
	break;

    case '~':
	printf( "        }\n"
		"\n"
		"        function reset() {\n"
		"            var i = 0;\n"
		"            while ((i|0) < (%d|0)) {\n",
		MEMSIZE * (bytecell?1:4)
		);
	if (bytecell)
	    printf(
		"                m[i] = 0;\n"
		"                i = (i + 1)|0\n"
		);
	else
	    printf(
		"                m[i>>2] = 0;\n"
		"                i = (i + 4)|0\n"
		);
	printf(
		"            }\n"
		"        }\n"
		"\n"
		"        return { reset: reset, run: run };\n"
		"    }\n"
		"\n"
		"    ctx[\"BFprogram\"] = BF(ctx, {\n"
		"        put: function (i) {\n"
		"                   document.getElementById(\"output\").innerHTML += String.fromCharCode(i);\n"
		"             }\n"
		"        , get: function () {\n"
		"                   var c = document.getElementById(\"input\").value.charCodeAt(inputPtr) || 0;\n"
		"                   inputPtr++;\n"
		"                   return c;\n"
		"               }\n"
		"    }, new ArrayBuffer(%d));\n"
		"})(this)\n", MEMSIZE * (bytecell?1:4));
	break;

    }
}
