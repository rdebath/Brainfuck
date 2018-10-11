#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

/*
 * PHP translation from BF, runs at about 41,000,000 instructions per second.
 */

static int ind = 0;
#define I printf("%*s", ind*4, "")

struct be_interface_s be_interface = {};

static void print_cstring(void);

static char *
cell(int mov)
{
    static char buf[9+3+sizeof(mov)*3];
    if (mov == 0) return "$m[$p]";
    if (mov < 0)
	sprintf(buf, "$m[$p-%d]", -mov);
    else
	sprintf(buf, "$m[$p+%d]", mov);
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
	printf( "%s%d%s%d%s",
		"<?php\n"
		"$m=array_fill(0, ",tapesz,", 0);\n"
		"$p=",tapeinit,";\n");
	break;

    case '=': I; printf("%s = %d;\n", mc, count); break;
    case 'B':
	if(bytecell) { I; printf("%s &= 255;\n", mc); }
	I; printf("$v = %s;\n", mc);
	break;
    case 'M': I; printf("%s = %s+$v*%d;\n", mc, mc, count); break;
    case 'N': I; printf("%s = %s-$v*%d;\n", mc, mc, count); break;
    case 'S': I; printf("%s = %s+$v;\n", mc, mc); break;
    case 'T': I; printf("%s = %s-$v;\n", mc, mc); break;

    case 'X': I; printf("fwrite(STDERR, \"Abort: Infinite Loop.\\n\"); exit;\n"); break;

    case '+': I; printf("%s += %d;\n", mc, count); break;
    case '-': I; printf("%s -= %d;\n", mc, count); break;
    case '<': I; printf("$p -= %d;\n", count); break;
    case '>': I; printf("$p += %d;\n", count); break;
    case '[':
	if(bytecell) { I; printf("%s &= 255;\n", mc); }
	I; printf("while(%s != 0){\n", mc);
	ind++;
	break;
    case ']':
	if (count > 0) {
            I; printf("$p += %d;\n", count);
        } else if (count < 0) {
            I; printf("$p -= %d;\n", -count);
        }
	if(bytecell) { I; printf("%s &= 255;\n", mc); }
	ind--; I; printf("}\n");
	break;
    case 'I':
	if(bytecell) { I; printf("%s &= 255;\n", mc); }
	I; printf("if(%s != 0){\n", mc);
	ind++;
	break;
    case 'E':
	if (count > 0) {
            I; printf("$p += %d;\n", count);
        } else if (count < 0) {
            I; printf("$p -= %d;\n", -count);
        }
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("print chr(%s&255);\n", mc); break;
    case ',': I; printf("$s = fread(STDIN, 1); if(strlen($s)) %s = ord($s);\n", mc); break;
    case '"': print_cstring(); break;
    }
}

static void
print_cstring(void)
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
	    I; printf("print \"%s\";\n", buf);
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
