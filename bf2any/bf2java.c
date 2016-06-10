#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Java translation From BF, runs at about ? instructions per second.
 *
 * Java has a limit of 64k on the size of a compiled function.
 * This is REALLY stupid and makes it completely unsuitable for some
 * applications. (Anything to do with automatically generated code
 * for example.)
 */

int ind = 0;
#define I        printf("%*s", ind*4, "")
#define prv(s,v) printf("%*s" s "\n", ind*4, "", (v))

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

	if (bytecell) {
	    printf("%s%d%s%d%s",
			"import java.io.InputStream;"
		"\n"	"import java.io.OutputStream;"
		"\n"	"import java.io.IOException;"
		"\n"
		"\n"	"public class fuck {"
		"\n"	"    static byte[] m;"
		"\n"	"    static int p;"
		"\n"	"    static byte v;"
		"\n"
		"\n"	"    private static void i() {"
		"\n"	"        try {"
		"\n"	"            System.in.read(m,p,1);"
		"\n"	"        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    private static void o() {"
		"\n"	"//        try {"
		"\n"	"            System.out.write(m[p]);"
		"\n"	"            System.out.flush();"
		"\n"	"//        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    public static void main(String[] args) {"
		"\n"	"        m=new byte[",tapesz,"];"
		"\n"	"        p=",tapeinit,";"
		"\n");

	} else {
	    printf("%s%d%s%d%s",
			"import java.io.InputStream;"
		"\n"	"import java.io.OutputStream;"
		"\n"	"import java.io.IOException;"
		"\n"
		"\n"	"public class fuck {"
		"\n"	"    static int[] m;"
		"\n"	"    static int p;"
		"\n"	"    static int v;"
		"\n"	"    static byte ch;"
		"\n"
		"\n"	"    private static void i() {"
		"\n"	"        try {"
		"\n"	"            v = System.in.read();"
		"\n"	"            if (v>=0) m[p] = v;"
		"\n"	"        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    private static void o() {"
		"\n"	"        ch = (byte) m[p];"
		"\n"	"        System.out.write(ch);"
		"\n"	"        System.out.flush();"
		"\n"	"    }"
		"\n"
		"\n"	"    public static void main(String[] args) {"
		"\n"	"        m=new int[",tapesz,"];"
		"\n"	"        p=",tapeinit,";"
		"\n");
	}

	ind+=2;
	break;

    case '~':
	ind--;
	I; printf("}\n");
	ind--;
	I; printf("}\n");
	break;

    case '=': I; printf("m[p] = %d;\n", count); break;
    case 'B': I; printf("v = m[p];\n"); break;
    case 'M': I; printf("m[p] += v*%d;\n", count); break;
    case 'N': I; printf("m[p] -= v*%d;\n", count); break;
    case 'S': I; printf("m[p] += v;\n"); break;
    case 'Q': I; printf("if(v!=0) m[p] = %d;\n", count); break;
    case 'm': I; printf("if(v!=0) m[p] += v*%d;\n", count); break;
    case 'n': I; printf("if(v!=0) m[p] -= v*%d;\n", count); break;
    case 's': I; printf("if(v!=0) m[p] += v;\n"); break;

    case 'X': I; printf("throw new IllegalStateException(\"Infinite Loop detected.\");"); break;

    case '+': I; printf("m[p] += %d;\n", count); break;
    case '-': I; printf("m[p] -= %d;\n", count); break;
    case '<': I; printf("p -= %d;\n", count); break;
    case '>': I; printf("p += %d;\n", count); break;

    case '[':
	I; printf("while(m[p] != 0) {\n");
	ind++;
	break;

    case ']':
	ind--;
	I; printf("}\n");
	break;

    case '.': I; printf("o();\n"); break;
    case ',': I; printf("i();\n"); break;

    case '"': print_cstring(); break;

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
		prv("System.out.println(\"%s\");", buf);
	    } else
		prv("System.out.print(\"%s\");", buf);
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
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

