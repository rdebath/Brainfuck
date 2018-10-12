#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Apple Swift translation From BF, runs at about ? instructions per second.
 */

static int ind = 0;
static FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))

struct be_interface_s be_interface = {};

static void print_string(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	ofd = stdout;

	pr(	"// WTF! Seriously!"
	"\n"	"// The standard fucking library isn't standard!!!"
	"\n"
	"\n"	"#if os(Linux) || os(FreeBSD)"
	"\n"	"    import Glibc"
	"\n"	"#endif"
	"\n"	"#if os(OSX) || os(iOS) || os(watchOS) || os(tvOS)"
	"\n"	"    import Darwin"
	"\n"	"#endif"
	"\n"	"#if os(SomeOtherPileOfShit)"
	"\n"	"    import FuckKnows"
	"\n"	"#endif"
	"\n");

	if (bytecell) {
	    prv("var mem = [CChar](count:%d, repeatedValue:CChar(0))", tapesz);
	    pr("var v:CChar = 0");
	} else {
	    prv("var mem = Array(count:%d, repeatedValue:0)", tapesz);
	    pr("var v = 0");
	}
	prv("var m = %d", tapeinit);
	pr("var c:Int32");

	break;

    case '~':
	break;

    case 'X':
	pr("assert(false, \"Abort: Infinite Loop.\")");
	pr("v = 0 ; mem[m] /= v");
	break;

    case '=': prv("mem[m] = %d", count); break;
    case 'B': pr("v = mem[m]"); break;
    case 'M': prv("mem[m] = mem[m] &+ v &* %d", count); break;
    case 'N': prv("mem[m] = mem[m] &- v &* %d", count); break;
    case 'S': pr("mem[m] = mem[m] &+ v"); break;
    case 'T': pr("mem[m] = mem[m] &- v"); break;
    case '*': pr("mem[m] = mem[m] &* v"); break;

    case 'C': prv("mem[m] = v &* %d", count); break;
    case 'D': prv("mem[m] = 0 &- v &* %d", count); break;
    case 'V': pr("mem[m] = v"); break;
    case 'W': pr("mem[m] = 0 &- v"); break;

    case '+': prv("mem[m] = mem[m] &+ %d", count); break;
    case '-': prv("mem[m] = mem[m] &- %d", count); break;
    case '>': prv("m += %d", count); break;
    case '<': prv("m -= %d", count); break;

    case '.': pr("putchar(Int32(mem[m]))"); break;
    case '"': print_string(); break;
    case ',':
	pr("c = getchar()");
	if (bytecell)
	    pr("if c >= 0 { mem[m] = CChar(c) }");
	else
	    pr("if c >= 0 { mem[m] = Int(c) }");
	break;

    case '[':
	if (bytecell) {
	    pr("while mem[m] != CChar(0) {");
	} else {
	    pr("while mem[m] != 0 {");
	}
	ind++;
	break;
    case ']': ind--; pr("}"); break;
    case 'I':
	if (bytecell) {
	    pr("if mem[m] != CChar(0) {");
	} else {
	    pr("if mem[m] != 0 {");
	}
	ind++;
	break;
    case 'E': ind--; pr("}"); break;
    }
}

static void
print_string(void)
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
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("print(\"%s\");", buf);
	    } else
		prv("print(\"%s\", terminator:\"\");", buf);
	    gotnl = 0; outlen = 0;
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
	    sprintf(buf2, "\\u{%x}", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}
