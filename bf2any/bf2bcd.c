#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bf2any.h"

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {
    .check_arg = fn_check_arg,
    .gen_code = gen_code,
    .disable_be_optim=1,
    .bytesonly = 1,
    .enable_chrtok = 1
};

static int bf_mov = 0;
static int state = 0;
static int col = 0;
static int maxcol = -1;

static const char bf[] = "><+-.,[]";

static const char * bcdbytebf[8] =
    {
	"0000",
	"AAAA",
	"01AA1AB08A7AC0B8A701AACAA",
	"01AAB08A7AC0B8A708A7AACAA8A7A",
	"000EAAAE",
	"000DAAAD",
	"01AAB08A7AC0B8A71BA8A7ACABAC0B8A7AA1CCA8A7AB1AA",
	"01AAB08A7AC0B8A71BA8A7ACABAC0B8A7AA1CCA8A7ACA"
    };

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "+init") == 0) {
	if (maxcol < 0) maxcol = 72;
    } else
    if (strcmp(arg, "-w") == 0) {
	maxcol = 0;
    } else
    if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	maxcol = atol(arg+2);
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-w99    Width to line wrap at, default 72"
	);
    } else
	return 0;
    return 1;
}

static void
pc_bcd2(int ch)
{

    if ((col>=((maxcol-2)/3)*3+2 && maxcol) || ch == '\n') {
	putchar('\n');
	col = 0;
	if (ch == ' ' || ch == '\n') ch = 0;
    }
    if (ch) {
	if (col != 0 && col%3 == 2) {
	    putchar(' ');
	    col++;
	}
	putchar(ch);
	col++;
    }
}

static void
pc_bcd(int ch)
{
    if (!enable_optim) { pc_bcd2(ch); return; }

    if (ch == '0') bf_mov++;
    else if (ch == 'A') bf_mov--;
    else {
	while (bf_mov>0) {bf_mov--; pc_bcd2('0'); }
	while (bf_mov<0) {bf_mov++; pc_bcd2('A'); }
	pc_bcd2(ch);
    }
}

static void
pmc_bcd(const char * s)
{
    while (*s) pc_bcd(*s++);
}

static void
gen_code(int ch, int count, char * strn)
{
    char * p;
    if ((p = strchr(bf,ch))) {
	const char * bcmd = bcdbytebf[p-bf];

	if ((ch == '+' || ch =='-') && count >= 16) {
	    count &= 0xFF;
	    if (ch == '-') count = 256 - count;
	    ch = '+';

	    while (count >= 16) {
		int hi = count / 16;
		if (hi>9) hi = 9;
		count -= hi*16;
		pmc_bcd("000");
		pc_bcd('0' + hi);
		pmc_bcd("AAAA");
		if (count == 0) return;
	    }
	}

	do {
	    pmc_bcd(bcmd);
	} while(--count>0);

	return;
    }

    if (ch == '=') {
	int nibble;
	for(nibble = 4; nibble>=0; nibble-=4) {
	    int d = (count>>nibble) & 0xF;
	    if (nibble == 4)
		pmc_bcd("000B1AC");
	    else
		pmc_bcd("B1AC");

	    if (d>=9) { pc_bcd('0'+d/2); pc_bcd('A'); d-= d/2; }
	    if (d>0) { pc_bcd('0'+d); pc_bcd('A'); }

	    if (nibble == 4)
		pmc_bcd("AAA");
	}
    }

    if (ch == '"' && strn && *strn) {
	int outch = 0, i, l = strlen(strn)+1;

	if (!state) pc_bcd('0');
	if (state) l--;
	for(i=0; i<l; i++)
        {
            int nibble;
	    int v = strn[i];

            for(nibble = 4; nibble>=0; nibble-=4) {
                int d, n = (v>>nibble) & 0xF;
                d = n - outch;
		if (n == 0 && v != 0 && d != 0) {
		    pmc_bcd("0EA");
		} else {
		    if (d<0) d += 0x10;
		    if (d>=9) { pc_bcd('0'+d/2); pc_bcd('A'); d-= d/2; }
		    if (d>0) { pc_bcd('0'+d); pc_bcd('A'); }
		    outch = n;
		    if(v) pc_bcd('E');
		}
            }
        }
	if (!state) pc_bcd('A');
    }

    if (ch == '~') {
	pc_bcd(0);
	if (col%3 == 1)
	    pc_bcd2('0');
	if (col)
	    pc_bcd2('\n');
    }

    if (ch == '!') state = count;

    if (ch == 'X')
	pmc_bcd("01ABCA");
}
