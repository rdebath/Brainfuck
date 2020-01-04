#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Nickle translation from BF, runs at about 12,000,000 instructions per second.
 */

static int ind = 0, use_utf8, hello_world = 0;
#define I printf("%*s", ind*2, "")

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_cstring(char * str);

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
	hello_world = !!count;
	if (!hello_world) {
	    ind ++;
	    if (tapelen <= 1)
		printf("%s%d%s",
		    "(func(){\n"
		    "  int[...] m;\n"
		    "  int v;\n"
		    "  int p = ", tapeinit, ";\n"
		    "  int n = -1;\n"
		    "  exception inf_loop(string msg);\n"
	            "  while(n<p) { n=n+1; m[n] = 0; }\n"
		);
	    else
		printf("%s%d%s%d%s",
		    "(func(){\n"
		    "  int[",tapesz,"] m = {0 ... };\n"
		    "  int v;\n"
		    "  int p = ", tapeinit, ";\n"
		    "  exception inf_loop(string msg);\n"
		);
	}
	break;
    case '~':
	if (!hello_world)
	    puts("})();");
	break;

    case '=': I; printf("m[p] = %d;\n", count); break;
    case 'B':
	if (bytecell) { I; puts("m[p] &= 255;"); }
	I; printf("v = m[p];\n");
	break;
    case 'M': I; printf("m[p] = m[p]+v*%d;\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d;\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v;\n"); break;
    case 'T': I; printf("m[p] = m[p]-v;\n"); break;
    case '*': I; printf("m[p] = m[p]*v;\n"); break;

    case 'C': I; printf("m[p] = v*%d;\n", count); break;
    case 'D': I; printf("m[p] = -v*%d;\n", count); break;
    case 'V': I; printf("m[p] = v;\n"); break;
    case 'W': I; printf("m[p] = -v;\n"); break;

    case 'X': I; printf("raise inf_loop (\"Aborting Infinite Loop.\");\n"); break;

    case '+': I; printf("m[p] = m[p] + %d;\n", count); break;
    case '-': I; printf("m[p] = m[p] - %d;\n", count); break;
    case '<': I; printf("p = p - %d;\n", count); break;
    case '>':
	I; printf("p = p + %d;\n", count);
	if (tapelen <= 1) {
	    I; printf("while(n<p) { n=n+1; m[n] = 0; }\n");
	}
	break;
    case '[':
	if (bytecell) { I; puts("m[p] &= 255;"); }
	I; printf("while (m[p] != 0) {\n");
	ind++;
	break;
    case ']':
	if (bytecell) { I; puts("m[p] &= 255;"); }
	ind--;
	I; printf("}\n");
	break;
    case 'I':
	if (bytecell) { I; puts("m[p] &= 255;"); }
	I; printf("if (m[p] != 0) {\n");
	ind++;
	break;
    case 'E':
	ind--;
	I; printf("}\n");
	break;
    case '.':
	if (use_utf8) {
	    I; printf("putchar(m[p]);\n");
	} else if (bytecell) {
	    I; printf("File::putb(m[p], stdout);\n");
	} else {
	    I; printf("File::putb(m[p] & 0xFF, stdout);\n");
	}
	I; printf("File::flush(stdout);\n");
	break;
    case '"': print_cstring(strn); break;
    case ',':
	I; puts("if (File::end(stdin) == false) {");
	I; puts("  m[p] = getchar();");
	I; puts("}");
	break;
    }
}

static void
print_cstring(char * str)
{
    char buf[256];
    int badchar = 0, gotperc = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (badchar == '\n' && !gotperc) {
		I; printf("printf(\"%s\\n\");\n", buf);
		badchar = 0;
	    } else if (badchar == '\n' && gotperc) {
		I; printf("printf(\"%%s\\n\", \"%s\");\n", buf);
		badchar = 0;
	    } else if (!gotperc) {
		I; printf("printf(\"%s\");\n", buf);
	    } else {
		I; printf("printf(\"%%s\", \"%s\");\n", buf);
	    }
	    outlen = gotperc = 0;
	    if (badchar == 0 && !hello_world) {
		I; printf("File::flush(stdout);\n");
	    }
	}
	if (badchar) {
	    I; printf("putchar(%d);\n", badchar);
	    badchar = 0;
	    I; printf("File::flush(stdout);\n");
	}
	if (!*str) break;

	if (*str == '%') gotperc = 1;
	if (*str == '"') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\\' && *str != '"') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
    }
}
