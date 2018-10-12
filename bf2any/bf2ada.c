
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Ada translation from BF, runs at about 3,300,000,000 instructions per second.
 */

static int ind = 0;
#define I printf("%*s", ind*4, "")

struct be_interface_s be_interface = {};

static const char * ctype = "mod 2**32";
static const char * charmod = " mod 256";
static int msk = -1;

static void print_cstring(void);

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	if (bytecell) { ctype = "mod 2**8"; charmod = ""; msk = 255; }

	printf("%s%s%s%d%s%d%s%s%s",
	    "with Ada.Text_IO,\n"
	    "     Ada.Strings.Unbounded;\n"
	    "\n"
	    "procedure Fuck is\n"
	    "    use Ada.Text_IO,\n"
	    "        Ada.Strings.Unbounded;\n"
	    "\n"
	    "    type Cell is ",ctype,";\n"
	    "    type Tape_Type is array (0 .. ",tapesz,") of Cell;\n"
	    "    m : Tape_Type := (others => 0);\n"
	    "    p : Natural := ",tapeinit,";\n"
	    "    v : Cell := 0;\n"
	    "    Infinite_loop : exception;\n"
	    "    GotEOF : Boolean := False;\n"
	    "    GotLine : Boolean := False;\n"
	    "    InpLine : Unbounded_String;\n"
	    "\n"
	    "procedure putch(ch : Cell) is\n"
	    "begin\n"
	    "    if ch = 10 then\n"
	    "        Put_Line(\"\");\n"
	    "    else\n"
	    "        Put (Character'Val (ch",charmod,"));\n"
	    "    end if;\n"
	    "end putch;\n"
	    "\n"
	    "function getch(oldch : Cell) return Cell is\n"
	    "    ch : Cell;\n"
	    "begin\n"
	    "    ch := oldch;\n"
	    "    if GotEOF then return ch ; end if;\n"
	    "    if Not GotLine then\n"
	    "        begin\n"
	    "            InpLine := To_Unbounded_String(Get_line);\n"
	    "            GotLine := True;\n"
	    "        exception\n"
	    "            when End_Error =>\n"
	    "                GotEOF := True;\n"
	    "                return oldch;\n"
	    "        end;\n"
	    "    end if;\n"
	    "    if InpLine = \"\" then\n"
	    "        GotLine := False;\n"
	    "        return 10;\n"
	    "    end if;\n"
	    "    ch := Character'Pos (Slice(InpLine, 1, 1)(1));\n"
	    "    InpLine := Delete(InpLine, 1, 1);\n"
	    "    return ch;\n"
	    "end getch;\n"
	    "\n"
	    "begin\n");

	ind++;
	break;

    case '~': ind--; printf("end Fuck;\n"); break;
    case '=': I; printf("m(p) := %d;\n", count & msk); break;
    case 'B': I; printf("v := m(p);\n"); break;
    case 'M': I; printf("m(p) := m(p)+v*%d;\n", count & msk); break;
    case 'N': I; printf("m(p) := m(p)-v*%d;\n", count & msk); break;
    case 'S': I; printf("m(p) := m(p)+v;\n"); break;
    case 'T': I; printf("m(p) := m(p)-v;\n"); break;
    case '*': I; printf("m(p) := m(p)*v;\n"); break;

    case 'C': I; printf("m(p) := v*%d;\n", count & msk); break;
    case 'D': I; printf("m(p) := -v*%d;\n", count & msk); break;
    case 'V': I; printf("m(p) := v;\n"); break;
    case 'W': I; printf("m(p) := -v;\n"); break;

    case 'X': I; printf("raise Infinite_loop;\n"); break;
    case '+': I; printf("m(p) := m(p) + %d;\n", count & msk); break;
    case '-': I; printf("m(p) := m(p) - %d;\n", count & msk); break;
    case '<': I; printf("p := p - %d;\n", count); break;
    case '>': I; printf("p := p + %d;\n", count); break;
    case '[': I; printf("while m(p) /= 0 loop\n"); ind++; break;
    case ']': ind--; I; printf("end loop;\n"); break;
    case 'I': I; printf("if m(p) /= 0 then\n"); ind++; break;
    case 'E': ind--; I; printf("end if;\n"); break;
    case '.': I; printf("putch (m(p));\n"); break;
    case '"': print_cstring(); break;
    case ',': I; printf("m(p) := getch(m(p));\n"); break;
    }
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    I;
	    if (gotnl)
		printf("Put_Line (\"%s\");\n", buf);
	    else
		printf("Put (\"%s\");\n", buf);
	    gotnl = 0; outlen = 0;
	}
	if (gotnl) {
	    I; printf("Put_Line (\"\");\n");
	    gotnl = 0;
	}
	if (badchar) {
	    I; printf("putch (%d);\n", badchar);
	    badchar = 0;
	}
	if (!*str) break;

	if (*str == '"') {
	    buf[outlen++] = '"'; buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~') {
	    buf[outlen++] = *str;
	} else if (*str == '\n') {
	    gotnl = 1;
	} else
	    badchar = (*str & 0xFF);
    }
}
