#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Vala translation from BF, runs at about 560,000,000 instructions per second.
 * Vala(-O2) translation from BF, runs at about 4,100,000,000 instructions per second.
 */

static int ind = 0;
static const char * cell_t = 0;
#define I printf("%*s", ind*2, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code, .cells_are_ints=1};

static void print_cstring(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	printf("void main () {\n");
	ind ++;
	/* Note: Using unsigned cells because vala inherits the broken
	 * int processing from gcc that is present without -fwrapv */

	if (!cell_t) {
	    if (bytecell)
		cell_t = "uint8";
	    else if (be_interface.cells_are_ints)
		cell_t = "uint";
	    else
		cell_t = "uint64";
	}
	if (!count)
	    printf("%s%s%s%d%s%d%s",
		"  ",cell_t," v, m[",tapesz,"];\n"
		"  int p = ", tapeinit, ";\n"
		"  v = 0; // STFU\n"
	    );
	break;
    case '~':
	puts("}");
	break;

    case '=':
	if (bytecell) {
	    I; printf("m[p] = %d;\n", count & 0xFF);
	} else {
	    I; printf("m[p] = %d;\n", count);
	}
	break;
    case 'B': I; printf("v = m[p];\n"); break;
    case 'M': I; printf("m[p] = m[p]+v*%d;\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d;\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v;\n"); break;
    case 'T': I; printf("m[p] = m[p]-v;\n"); break;
    case '*': I; printf("m[p] = m[p]*v;\n"); break;
    case '/': I; printf("m[p] = m[p]/v;\n"); break;
    case '%': I; printf("m[p] = m[p]%%v;\n"); break;

    case 'C': I; printf("m[p] = v*%d;\n", count); break;
    case 'D': I; printf("m[p] = -v*%d;\n", count); break;
    case 'V': I; printf("m[p] = v;\n"); break;
    case 'W': I; printf("m[p] = -v;\n"); break;

    case 'X': I; printf("error (\"Aborting Infinite Loop.\");\n"); break;

    case '+': I; printf("m[p] = m[p] + %d;\n", count); break;
    case '-': I; printf("m[p] = m[p] - %d;\n", count); break;
    case '<': I; printf("p = p - %d;\n", count); break;
    case '>': I; printf("p = p + %d;\n", count); break;
    case '[': I; printf("while (m[p] != 0) {\n"); ind++; break;
    case ']': ind--; I; printf("}\n"); break;
    case 'I': I; printf("if (m[p] != 0) {\n"); ind++; break;
    case 'E': ind--; I; printf("}\n"); break;
    case '.':
	I; printf("stdout.putc ((char) m[p]);\n");
	I; printf("stdout.flush ();\n");
	break;
    case '"': print_cstring(strn); break;
    case ',':
	I; puts("{");
	ind++;
	I; puts("int c;");
	I; puts("c = stdin.getc ();");
	I; puts("if (stdin.eof () == false) {");
	I; puts("  m[p] = (uint8) c;");
	I; puts("} else {");
	I; puts("  stdin.clearerr ();");
	I; puts("}");
	ind--;
	I; puts("}");
	break;
    }
}

static void
print_cstring(char * str)
{
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n') {
		I; printf("stdout.puts (\"%s\\n\");\n", buf);
		badchar = 0;
	    } else {
		I; printf("stdout.puts (\"%s\");\n", buf);
		I; printf("stdout.flush ();\n");
	    }
	    outlen = 0;
	}
	if (badchar) {
	    I; printf("stdout.putc (%d);\n", badchar);
	    I; printf("stdout.flush ();\n");
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '"') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\\' && *str != '"') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
