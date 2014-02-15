#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * S-Lang translation from BF, runs at about 8,000,000 instructions per second.
 */

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")

static void print_cstring(void);

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	printf("#!/usr/bin/env slsh\n");
	I; printf("variable p=%d;\n",BOFF);
	I; printf("variable v;\n");
	I;
	if (bytecell)
	    printf("variable m = UChar_Type [30000];\n");
	else
	    printf("variable m = Integer_Type [30000];\n");
	I; printf("variable goteof = 0, gotline = 0, line = \"\", c;\n");
	break;

    case '=': I; printf("m[p] = %d;\n", count); break;
    case 'B': I; printf("v = m[p];\n"); break;
    case 'M': I; printf("m[p] = m[p]+v*%d;\n", count); break;
    case 'N': I; printf("m[p] = m[p]-v*%d;\n", count); break;
    case 'S': I; printf("m[p] = m[p]+v;\n"); break;
    case 'Q': I; printf("if (v != 0) m[p] = %d;\n", count); break;
    case 'm': I; printf("if (v != 0) m[p] = m[p]+v*%d;\n", count); break;
    case 'n': I; printf("if (v != 0) m[p] = m[p]-v*%d;\n", count); break;
    case 's': I; printf("if (v != 0) m[p] = m[p]+v;\n"); break;

    case 'X': I; printf("throw LimitExceededError, \"Abort: Infinite Loop.\\n\";\n"); break;

    case '+': I; printf("m[p] += %d;\n", count); break;
    case '-': I; printf("m[p] -= %d;\n", count); break;
    case '<': I; printf("p -= %d;\n", count); break;
    case '>': I; printf("p += %d;\n", count); break;
    case '[': I; printf("while(m[p]!=0){\n"); ind++; break;
    case ']': ind--; I; printf("}\n"); break;

    case '~': break;

    case '.': I; printf("()=printf(\"%%c\",m[p]);\n"); break;
    case '"': print_cstring(); break;

    case ',':
	I; printf("while(1) {\n");
	I; printf("    if (goteof) break;\n");
	I; printf("    if (gotline == 0) {\n");
	I; printf("        gotline = fgets(&line, stdin);\n");
	I; printf("        if (gotline<0) goteof = 1;\n");
	I; printf("        if (goteof) break;\n");
	I; printf("        gotline = 1;\n");
	I; printf("    }\n");
	I; printf("    if (line == \"\") {\n");
	I; printf("        gotline=0;\n");
	I; printf("        continue;\n");
	I; printf("    }\n");
	I; printf("    c = substr(line, 1, 1);\n");
	I; printf("    line=substr(line, 2, -1);\n");
	I; printf("    sscanf(c, \"%%c\", &(m[p]));\n");
	I; printf("    break;\n");
	I; printf("}\n");
	break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0, gotperc = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		I; printf("message(\"%s\");\n", buf);
	    } else if (gotperc) {
		I; printf("printf(\"%%s\",\"%s\");\n", buf);
	    } else {
		I; printf("printf(\"%s\");\n", buf);
	    }
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '%') gotperc = 1;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    buf[outlen++] = *str;
	} else if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
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
