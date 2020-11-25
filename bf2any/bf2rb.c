#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

/*
 * Ruby translation from BF, runs at about 31,000,000 instructions per second.
 */

static int ind = 0;
#define I printf("%*s", ind*4, "")
static int safetapeoff = 0, curtapeoff = 0;
static int utf8_conv = 0;
static int unbounded_tape = 0;
static int bpc = 0;

static void print_cstring(char * str);

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {.check_arg=fn_check_arg, .gen_code=gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-M") == 0) {
	unbounded_tape = 1;
	return 1;
    }
    if (strcmp(arg, "-u") == 0) {
	utf8_conv = 1;
	return 1;
    }
    if (strncmp(arg, "-b", 2) == 0 && arg[2] > '0' && arg[2] <= '9') {
	bpc = strtol(arg+2, 0, 10);
	if (bpc < 32) {
	    fprintf(stderr, "Cell size must be a minimum of 32 bits\n");
	    exit(1);
	}
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
    const char * cm;

    if (!unbounded_tape) move_opt(&ch, &count, &mov);
    if (ch == 0) return;

    cm = cell(mov);
    switch(ch) {
    case '!':
	puts("#!/usr/bin/ruby");
	if (!count)
	{
	    /* Using push is about 20% slower, using a Hash is a LOT slower! */
	    if (!unbounded_tape)
		printf("m = Array.new(%d, 0)\n",tapesz);
	    else {
		puts("m = Array.new");
	    }
	    printf("%s%d%s", "p = ", tapeinit, "\n");
	    if (unbounded_tape)
		puts("m.push(0) while p>=m.length");
	    if (bpc)
		printf("%s%d%s", "msk = 2**", bpc, "\n");
	}
	break;

    case '=': I; printf("%s = %d\n", cm, count); break;
    case 'B':
	if(bytecell) { I; printf("%s &= 255\n", cm); }
	else if(bpc) { I; printf("%s %%= msk\n", cm); }
	I; printf("v= %s\n", cm);
	break;
    case 'M': I; printf("%s += v*%d\n", cm, count); break;
    case 'N': I; printf("%s -= v*%d\n", cm, count); break;
    case 'S': I; printf("%s += v\n", cm); break;
    case 'T': I; printf("%s -= v\n", cm); break;
    case '*': I; printf("%s *= v\n", cm); break;
    case '/': I; printf("%s /= v\n", cm); break;
    case '%': I; printf("%s %%= v\n", cm); break;

    case 'C': I; printf("%s = v*%d\n", cm, count); break;
    case 'D': I; printf("%s = -v*%d\n", cm, count); break;
    case 'V': I; printf("%s = v\n", cm); break;
    case 'W': I; printf("%s = -v\n", cm); break;

    case 'X': I; printf("raise 'Aborting Infinite Loop.'\n"); break;

    case '+': I; printf("%s += %d\n", cm, count); break;
    case '-': I; printf("%s -= %d\n", cm, count); break;
    case '<':
	I; printf("p -= %d\n", count);
	curtapeoff -= count;
	break;
    case '>':
	I; printf("p += %d\n", count);
	curtapeoff += count;
	if (unbounded_tape && curtapeoff > safetapeoff) {
	    safetapeoff = curtapeoff;
	    I; puts("m.push(0) while p>=m.length");
	}
	break;
    case '[':
	if(bytecell) { I; printf("%s &= 255\n", cm); }
	else if(bpc) { I; printf("%s %%= msk\n", cm); }
	I; printf("while %s != 0\n", cm);
	ind++;
	curtapeoff = safetapeoff = 0;
	break;
    case ']':
	if (count > 0) {
            I; printf("p += %d\n", count);
        } else if (count < 0) {
            I; printf("p -= %d\n", -count);
	}

	if(bytecell) { I; printf("%s &= 255\n", cm); }
	else if(bpc) { I; printf("%s %%= msk\n", cm); }
	ind--; I; printf("end\n");
	curtapeoff = safetapeoff = 0;
	break;
    case 'I':
	if(bytecell) { I; printf("%s &= 255\n", cm); }
	else if(bpc) { I; printf("%s %%= msk\n", cm); }
	I; printf("if %s != 0\n", cm);
	ind++;
	curtapeoff = safetapeoff = 0;
	break;
    case 'E':
	if (count > 0) {
            I; printf("p += %d\n", count);
        } else if (count < 0) {
            I; printf("p -= %d\n", -count);
	}
	ind--; I; printf("end\n");
	curtapeoff = safetapeoff = 0;
	break;
    case '"': print_cstring(strn); break;
    case '.':
	I;
	if(!utf8_conv) printf("print (%s & 255).chr\n", cm);
	else {
	    if(bytecell) { I; printf("%s &= 255\n", cm); }
	    else if(bpc) { I; printf("%s %%= msk\n", cm); }
	    printf("begin print '' << %s rescue print (%s & 255).chr end\n", cm, cm);
	}
	break;
    case ',':
	I;
	if (!utf8_conv)
	    printf("%s = $stdin.getbyte if !$stdin.eof\n", cm);
	else
	    printf("%s = $stdin.getc.ord if !$stdin.eof\n", cm);
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
	    if (gotnl) {
		buf[outlen-2] = 0;
		I; printf("puts \"%s\"\n", buf);
	    } else {
		I; printf("print \"%s\"\n", buf);
	    }
	    gotnl = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '"' || *str == '\\' || *str == '#') {
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
