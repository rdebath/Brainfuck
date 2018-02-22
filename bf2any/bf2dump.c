#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = { .check_arg = fn_check_arg};
static int c_header = 0;

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-no_if") == 0) { be_interface.noifcmd = 1; return 1; }
    if (strcmp(arg, "-int") == 0) { be_interface.cells_are_ints = 1; return 1; }
    if (strcmp(arg, "-c") == 0) { c_header = 1; return 1; }
    if (strcmp(arg, "-#") == 0) return 1;
    return 0;
}

static char *
cell(int mov)
{
    static char buf[3+3+sizeof(mov)*3];
    if (mov == 0) return "*m";
    sprintf(buf, "m[%d]", mov);
    return buf;
}

void
outcmd(int ch, int count)
{
static int ind = 0;
    int mov = 0;

    if (ch == '!' && c_header && bytecell)
	printf("%s\n",  "#include<stdio.h>"
		"\n"	"#define I(a,b)"
		"\n"	"#define putch(x) putchar(x)"
		"\n"	"#define getch(x) {int a; a=getchar();if(a!=EOF) x=a;}"
		"\n"	"static unsigned char mem[30000], *m=mem, v;"
		"\n"	"int"
		"\n"	"main()");

    if (ch == '!' && c_header && !bytecell)
	printf("%s\n",  "#include<stdio.h>"
		"\n"	"#define I(a,b)"
		"\n"	"#define putch(x) putchar((char)x)"
		"\n"	"#define getch(x) {int a; a=getchar();if(a!=EOF) x=a;}"
		"\n"	"static unsigned int mem[30000], *m=mem, v;"
		"\n"	"int"
		"\n"	"main()");

    printf("I('%c', %d)\t", ch, count);

    move_opt(&ch, &count, &mov);
    if (ch == 0) { putchar('\n'); return; }

    if (ch == ']' || ch == '~' || ch == 'E') ind--;
    printf("%*s", ind*2, "");

    switch(ch) {
    case '=': printf("%s = %d;\n", cell(mov), count); break;
    case 'B': printf("v = %s;\n", cell(mov)); break;
    case 'M': printf("%s += v*%d;\n", cell(mov), count); break;
    case 'N': printf("%s -= v*%d;\n", cell(mov), count); break;
    case 'S': printf("%s += v;\n", cell(mov)); break;
    case 'Q': printf("if(v!=0) %s = %d;\n", cell(mov), count); break;
    case 'm': printf("if(v!=0) %s += v*%d;\n", cell(mov), count); break;
    case 'n': printf("if(v!=0) %s -= v*%d;\n", cell(mov), count); break;
    case 's': printf("if(v!=0) %s += v;\n", cell(mov)); break;

    case '>': printf("m += %d;\n", count); break;
    case '<': printf("m -= %d;\n", count); break;
    case '+': printf("%s += %d;\n", cell(mov), count); break;
    case '-': printf("%s -= %d;\n", cell(mov), count); break;
    case '.': printf("putch(%s);\n", cell(mov)); break;
    case ',': printf("getch(%s);\n", cell(mov)); break;

    case 'X':
	printf("fprintf(stderr, \"Abort: Infinite Loop.\\n\"); exit(1);\n");
	break;

    case '[':
	printf("while (%s!=0) {\n", cell(mov));
	ind++;
	break;

    case ']':
	if (count > 0)
	    printf("  m += %d;\n%9s\t%*s", count, "", ind*2, "");
	else if (count < 0)
	    printf("  m -= %d;\n%9s\t%*s", -count, "", ind*2, "");
	printf("} /* %s */\n", cell(mov));
	break;

    case 'I':
	printf("if (%s!=0) {\n", cell(mov));
	ind++;
	break;
    case 'E':
	if (count > 0)
	    printf("  m += %d;\n%9s\t%*s", count, "", ind*2, "");
	else if (count < 0)
	    printf("  m -= %d;\n%9s\t%*s", -count, "", ind*2, "");
	printf("} /* %s */\n", cell(mov));
	break;

    case '"':
	{
	    char * s = get_string(), *p=s;
	    int percent = 0;
	    for(p=s;*p && !percent;p++) percent = (*p == '%');
	    printf("printf(%s\"", percent?"\"%s\",":"");
	    for(;*s;s++)
		if (*s>=' ' && *s<='~' && *s!='\\' && *s!='"')
		    putchar(*s);
		else if (*s == '\n')
		    printf("\\n");
		else if (*s == '\\' || *s == '"')
		    printf("\\%c", *s);
		else
		    printf("\\%03o", *s);
	    printf("\");\n");
	}
	break;

    case '!': puts("{"); ind++; break;
    case '~': puts("}"); break;

    default:
	printf("/* ? */\n");
	break;
    }
}
