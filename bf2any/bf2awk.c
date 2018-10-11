#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

/*
 * MAWK translation from BF, runs at about 23,000,000 instructions per second.
 * GAWK translation from BF, runs at about 13,000,000 instructions per second.
 *
 *  Getting character I/O is a little interesting here; but otherwise the
 *  language is very similar to C (without semicolons).
 *
 *  Generated code works in most awk implementations. For really old ones
 *  without functions the getch() function has to be inlined.
 */

/* Do this by default as getch() is rare. */
#define INLINEGETCH

static int do_input = 0;
static int ind = 0;
static int old_awk = 0;
#define I printf("%*s", ind*4, "")

static void print_cstring(void);

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {.check_arg=fn_check_arg};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-fn") == 0) {
	old_awk = 0;
	return 1;
    }
    if (strcmp(arg, "-foawk") == 0) {
	old_awk = 1;
	return 1;
    }
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-foawk  Use only oawk features (very old)");
	return 1;
    } else
    return 0;
}

static char *
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

void
outcmd(int ch, int count)
{
    int mov = 0;
    char * cm;

    move_opt(&ch, &count, &mov);
    if (ch == 0) return;

    cm = cell(mov);
    switch(ch) {
    case '!':
	printf("#!/usr/bin/awk -f\n");
	printf("BEGIN {\n"); ind++;
	I; printf("p=%d\n",tapeinit); break;
	break;

    case '=': I; printf("%s = %d\n", cm, count); break;
    case 'B':
	if(bytecell) { I; printf("%s %%= 256\n", cm); }
	I; printf("v = %s\n", cm);
	break;
    case 'M': I; printf("%s += v*%d\n", cm, count); break;
    case 'N': I; printf("%s -= v*%d\n", cm, count); break;
    case 'S': I; printf("%s += v\n", cm); break;
    case 'T': I; printf("%s -= v\n", cm); break;

    case 'X': I; printf("print \"Abort: Infinite Loop.\\n\" >\"/dev/stderr\"; exit;\n"); break;

    case '+': I; printf("%s += %d\n", cm, count); break;
    case '-': I; printf("%s -= %d\n", cm, count); break;
    case '<': I; printf("p -= %d\n", count); break;
    case '>': I; printf("p += %d\n", count); break;
    case '[':
	if(bytecell) { I; printf("%s %%= 256\n", cm); }
	I; printf("while(%s!=0){\n", cm);
	ind++;
	break;
    case ']':
	if (count > 0) {
	    I; printf("p += %d\n", count);
	} else if (count < 0) {
	    I; printf("p -= %d\n", -count);
	}

	if(bytecell) { I; printf("%s %%= 256\n", cm); }
	ind--;
	I; printf("}\n");
	break;
    case 'I':
	if(bytecell) { I; printf("%s %%= 256\n", cm); }
	I; printf("if(%s!=0){\n", cm);
	ind++;
	break;
    case 'E':
	if (count > 0) {
	    I; printf("p += %d\n", count);
	} else if (count < 0) {
	    I; printf("p -= %d\n", -count);
	}

	ind--;
	I; printf("}\n");
	break;

    case '~':
	I; printf("exit 0\n");
	ind--;
	printf("}\n");

	if (do_input) {
	    printf("\n");
	    printf("function getch(mov) {\n");
	    printf("    if (goteof) return;\n");
	    printf("    if (!gotline) {\n");
	    printf("        gotline = getline line\n");
	    printf("        goteof = !gotline\n");
	    printf("        if (goteof) return\n");
	    printf("    }\n");
	    printf("    if (line == \"\") {\n");
	    printf("        gotline=0\n");
	    printf("        m[p+mov]=10\n");
	    printf("        return\n");
	    printf("    }\n");
	    printf("    if (!genord) {\n");
	    printf("        for(i=1; i<256; i++)\n");
	    printf("            ord[sprintf(\"%%c\",i)] = i\n");
	    printf("        genord=1\n");
	    printf("    }\n");
	    printf("    c = substr(line, 1, 1)\n");
	    printf("    line=substr(line, 2)\n");
	    printf("    m[p+mov] = ord[c]\n");
	    printf("}\n");
	}
	break;

    case '.':
	if (old_awk) {
	    I; printf("printf \"%%c\",((%s %% 256) + 256) %% 256\n", cm);
	} else {
	    if(bytecell) { I; printf("%s %%= 256\n", cm); }
	    I; printf("printf \"%%c\",%s\n", cm);
	}
	break;
    case '"': print_cstring(); break;

    case ',':
	if (!old_awk) {
	    I; printf("getch(%d)\n", mov); do_input++;
	    break;
	}

	I; printf("while(1) {\n");
	I; printf("    if (goteof) break\n");
	I; printf("    if (!gotline) {\n");
	I; printf("        gotline = getline\n");
	I; printf("        if(!gotline) goteof = 1\n");
	I; printf("        if (goteof) break\n");
	I; printf("        line = $0\n");
	I; printf("    }\n");
	I; printf("    if (line == \"\") {\n");
	I; printf("        gotline=0\n");
	I; printf("        %s=10\n", cm);
	I; printf("        break\n");
	I; printf("    }\n");
	I; printf("    if (!genord) {\n");
	I; printf("        for(i=1; i<256; i++)\n");
	I; printf("            ord[sprintf(\"%%c\",i)] = i\n");
	I; printf("        genord=1\n");
	I; printf("    }\n");
	I; printf("    c = substr(line, 1, 1)\n");
	I; printf("    line=substr(line, 2)\n");
	I; printf("    %s = ord[c]\n", cm);
	I; printf("    break\n");
	I; printf("}\n");
	break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0, gotperc = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		I; printf("print \"%s\"\n", buf);
	    } else if (gotperc) {
		I; printf("printf \"%%s\",\"%s\"\n", buf);
	    } else {
		I; printf("printf \"%s\"\n", buf);
	    }
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (badchar) {
	    I; printf("printf \"%%c\",%d\n", badchar);
	    badchar = 0;
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
	} else if (old_awk) {
	    badchar = (*str & 0xFF);
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
