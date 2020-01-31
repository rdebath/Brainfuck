#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

/*
 * Bc translation from BF, runs at about 4,100,000 instructions per second.
 */

static int ind = 0;
#define I printf("%*s", ind*4, "")
static int used_o = 0;

static gen_code_t gen_code;
struct be_interface_s be_interface = {.gen_code=gen_code};

static void print_cstring(const char * str);

static const char *
cell(int mov)
{
    static char buf[9+3+sizeof(mov)*3];
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
    int mov = 0, i;
    const char * mc;

    move_opt(&ch, &count, &mov);
    if (ch == 0) return;

    mc = cell(mov);
    switch(ch) {

    case '=': I; printf("%s = %d\n", mc, count); break;
    case 'B':
	if(bytecell) { I; printf("%s %%= 256\n", mc); }
	I; printf("v = %s\n", mc);
	break;
    case 'M': I; printf("%s = %s+v*%d\n", mc, mc, count); break;
    case 'N': I; printf("%s = %s-v*%d\n", mc, mc, count); break;
    case 'S': I; printf("%s = %s+v\n", mc, mc); break;
    case 'T': I; printf("%s = %s-v\n", mc, mc); break;
    case '*': I; printf("%s = %s*v\n", mc, mc); break;

    case 'C': I; printf("%s = v*%d\n", mc, count); break;
    case 'D': I; printf("%s = -v*%d\n", mc, count); break;
    case 'V': I; printf("%s = v\n", mc); break;
    case 'W': I; printf("%s = -v\n", mc); break;

    case 'X':
	print_cstring("STOP Command\n");
	I; puts("return(0)");
	used_o = 1;
	break;

    case '+': I; printf("%s += %d\n", mc, count); break;
    case '-': I; printf("%s -= %d\n", mc, count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if (bytecell) { I; printf("%s %%= 256\n", mc); }
	I; printf("while(%s != 0){\n", mc);
	ind++;
	break;
    case ']':
	if (count > 0) {
	    I; printf("p += %d\n", count);
	} else if (count < 0) {
	    I; printf("p -= %d\n", -count);
	}
	if (bytecell) { I; printf("%s %%= 256\n", mc); }
	ind--; I; printf("}\n");
	break;
    case 'I':
	if (bytecell) { I; printf("%s %%= 256\n", mc); }
	I; printf("if(%s != 0){\n", mc);
	ind++;
	break;
    case 'E':
	if (count > 0) {
	    I; printf("p += %d\n", count);
	} else if (count < 0) {
	    I; printf("p -= %d\n", -count);
	}
	ind--; I; printf("}\n");
	break;
    case '.': I; printf("v = o(%s)\n", mc); used_o = 1; break;
    case ',':
	I; puts("\"Input command unimplementable.\n\"\nreturn(0)\n");
	I; printf("%s = i(%s)\n", mc, mc); break;
	break;
    case '"': print_cstring(strn); break;

    case '!':
	puts( "define x() {");
	ind++;
	if (!count) {
	    I; printf("%s%d%s", "p = ", tapeinit, "\n");
	}
	break;

    case '~':
	ind--;
	puts("}");

	if (used_o) {
	    puts("\ndefine o(c) {"
		"\n" "  c = ((c % 256) + 256 ) % 256"
		"\n" "  if(c == 10) \""
		"\n" "\"");

	    puts("");
	    printf("  if(c == %d) \"%c\"\n", 27, 27);
	    printf("  if(c == %d) \"%c\"\n", '"', '\'');
	    puts("");

	    for(i=' '; i<='~'; i++)
		if (i != '"')
		    printf("  if(c == %d) \"%c\"\n", i,i);

	    puts("");
	    printf("  if(c == %d) \"%c\"\n", 127, 127);
	    puts("");
	    for(i=128; i<256; i++)
		printf("  if(c == %d) \"%c\"\n", i,i);
	    puts("}");
	}

	puts("\nv = x()\nquit");
	break;
    }
}

static void
print_cstring(const char * str)
{
    char buf[256];
    int badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    I; printf("\"%s\"\n", buf);
	    outlen = 0;
	}
	if (badchar) {
	    I; printf("v = o(%d)\n", badchar & 0xFF);
	    used_o = 1;
	    badchar = 0;
	}
	if (!*str) break;

	if ((*str == '"' || *str < ' ' || *str > '~') && *str != '\n')
	    badchar = *str;
	else
	    buf[outlen++] = *str;
    }
}
