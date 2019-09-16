#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if _POSIX_VERSION < 200112L && _XOPEN_VERSION < 500
#define NO_SNPRINTF
#endif

#include "bf2any.h"

/*
 * Unix/GNU dc translation from BF, runs at about 1,500,000 instructions per second.
		  If malloc is avoided ...        2,600,000
 */

/*
 *  There are two other methods of storing the tape.
 *
 *  1)  Use a register to stack the left of the tape and the main stack
 *      for the right hand tape.
 *      This cannot use run length encoding for tape movements.
 *
 *  2)  I could use a pair of variables and use divmod to move bytes from
 *      one to the other. This means we can't have bignum cells but
 *      run length encoding is possible. (divmod by 256^N)
 */

static int do_run = 0;
static int do_output = 0;
static int do_input = 0;
static int do_trailer = 0;
static int max_input_depth = 0;
static int ind = 0;
#define I printf("%*s", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")

static void print_dcstring(char * str);

static FILE * ofd;
static int outputmode = 2;
static int inputmode = 0;
static int stackdepth = 0;

static check_arg_t fn_check_arg;
static gen_code_t gen_code;
struct be_interface_s be_interface = {fn_check_arg, gen_code};

static int
fn_check_arg(const char * arg)
{
    if (strncmp(arg, "-i", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	inputmode = atol(arg+2); return 1;
    } else if (strcmp(arg, "-t") == 0) {
	outputmode=1; return 1;
    } else if (strcmp(arg, "-g") == 0) {
	outputmode=2; return 1;
    } else if (strcmp(arg, "-a") == 0) {
	outputmode=3; return 1;
    } else if (strcmp(arg, "-sed") == 0) {
	outputmode=5; return 1;
#ifndef _WIN32
    } else if (strcmp(arg, "-r") == 0) {
	do_run=inputmode=1; return 1;
#endif
    } else if (strcmp(arg, "-d") == 0) {
	inputmode=0; return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-d      Dump code"
#ifndef _WIN32
	"\n\t"  "-r      Run program (standard input will be translated)"
#endif
	"\n\t"  "-i0     ',' command will error"
	"\n\t"  "-i1     ',' command will be dc '?' command"
	"\n\t"  "-i2     ',' command input will be requested"
	"\n\t"  "-i3     ',' command will be ignored"
	"\n\t"  "-i4     ',' command will be dc '$' command"
	"\n\t"  "-t      Traditional dc output; works with V7 unix dc."
	"\n\t"  "-g      General modern dc output. (BSD or GNU)"
	"\n\t"  "-a      Autodetect dc output type V7 or modern."
	"\n\t"  "-sed    For dc.sed.");
	return 1;
    } else
	return 0;
}

#ifndef _WIN32
/* Note: calling exit() isn't posix */
static void endprog(int s) { exit(s != SIGCHLD); }
#endif

static char *
dc_ltoa(long val)
{
    static char buf[64];
#ifndef NO_SNPRINTF
    snprintf(buf, sizeof(buf), "%ld", val);
#else
    sprintf(buf, "%ld", val);
#endif
    if (*buf == '-') *buf = '_';
    return buf;
}

static void
gen_code(int ch, int count, char * strn)
{
    switch(ch) {
    case '!':
#ifndef _WIN32
	if (do_run) ofd = popen("dc", "w"); else
#endif
	    ofd = stdout;
	if (outputmode == 2) {
	    if (!count || do_run) {
		pr("#!/usr/bin/dc");
		prv("[%dsp", tapeinit);
		do_trailer = 1;
	    }
	} else {
	    prv("[%dsp", tapeinit);
	    do_trailer = 1;
	}
	break;

    case '=': prv("%slp:a", dc_ltoa(count)); break;
    case 'B':
	if(bytecell)
	    pr("lmxsV");
	else
	    pr("lp;asV");
	break;
    case 'M': prv("lp;alV%s*+lp:a", dc_ltoa(count)); break;
    case 'N': prv("lp;alV%s*+lp:a", dc_ltoa(-count)); break;
    case 'S': pr("lp;alV+lp:a"); break;
    case 'T': pr("lp;alV-lp:a"); break;
    case '*': pr("lp;alV*lp:a"); break;

    case 'C': prv("lV%s*lp:a", dc_ltoa(count)); break;
    case 'D': prv("lV%s*lp:a", dc_ltoa(-count)); break;
    case 'V': pr("lVlp:a"); break;
    case 'W': pr("0lV-lp:a"); break;

    case '+': prv("lp;a%s+lp:a", dc_ltoa(count)); break;
    case '-': prv("lp;a%s-lp:a", dc_ltoa(count)); break;
    case '<': prv("lp%s-sp", dc_ltoa(count)); break;
    case '>': prv("lp%s+sp", dc_ltoa(count)); break;
    case '[': pr("["); stackdepth++; break;
    case ']':
	if(bytecell)
	    pr("lmx0!=b]Sblmx0!=bLbc");
	else
	    pr("lp;a0!=b]Sblp;a0!=bLbc");
	stackdepth--;
	break;

    case 'I': pr("["); stackdepth++; break;
    case 'E':
	if(bytecell)
	    pr("]Sblmx0!=bLbc");
	else
	    pr("]Sblp;a0!=bLbc");
	stackdepth--;
	break;

    case ',':
	pr("lix");
	do_input=1;
	if(stackdepth>max_input_depth) max_input_depth=stackdepth;
	break;

    case 'X':
	pr("[STOP command executed\n]P");
	if (stackdepth>1)
	    prv("%dQ", stackdepth);
	else
	    pr("q");
	break;

    case '.': if (outputmode == 2) pr("lp;aaP"); else pr("lox"); do_output=1; break;
    case '"': print_dcstring(strn); break;
    }

    if (ch != '~') return;

    if (do_trailer)
	pr("q]SF");

    if ((do_output && outputmode != 2) || bytecell)
	fprintf(ofd, "[256+]sM [lp;a 256 %% d0>M]sm\n");

    if (do_output)
    {
	if (outputmode&1) {
	    int i;
	    if (outputmode & 2)
		fprintf(ofd, "[");
	    for (i=0; i<256; i++) {
		if (i == '\n') {
		    fprintf(ofd, "[[\n]P]%d:C\n", i);
		} else
		if ( i >= ' ' && i <= '~' && i != '[' && i != ']'
		    && i != '\\' /* bsd dc */
		    && (outputmode != 5 || (i != '|' && i != '~')) /* dc.sed */
		    ) {
		    fprintf(ofd, "[[%c]P]%d:C", i, i);
		} else
		if (i < 100) {
		    fprintf(ofd, "[%dP]%d:C", i, i);
		} else if ( i >= 127 ) {
		    fprintf(ofd, "[[%c]P]%d:C", i, i);
		} else
		    fprintf(ofd, "[[<%d>]P]%d:C", i, i); /* Give up */
		if (i%8 == 7) fprintf(ofd, "\n");
	    }
	    if (outputmode & 2)
		fprintf(ofd, "]sZ");
	    fprintf(ofd, "\n");
	}

	switch(outputmode)
	{
	case 1: case 5:
	    fprintf(ofd, "[;Cx]sO [lmx;Cx]so\n");
	    break;

	case 3:
	    /*  Note: dc.sed works in traditional mode, but as it takes
		minutes just to print "Hello World!" without optimisation
		there's not much point doing an auto detection. However,
		if wanted the best method seems to be to check that the
		comment indicator '#' also works for new dc(1). */

	    /* Use 'Z' command. Detects dc.sed as new style. */
	    fprintf(ofd, "[1   [aP]sO  [lmxaP]so ]sB\n");
	    fprintf(ofd, "[lZx [;Cx]sO [lmx;Cx]so ]sA\n");
	    fprintf(ofd, "0 0 [o] Z 1=B 0=A 0sA 0sB 0sZ c\n");
	    break;
	}
    }

    if (do_input && inputmode == 1) {
	pr("[[INPUT command failed to execute.\n]P");
	prv("%dQ]sQ", max_input_depth+3);
	pr("[?z0=Qlp:a]si");
	do_input = 0;
	if (do_run) do_run = 2;
    }

    if (do_input && inputmode == 2) {
	int flg = 0;
	pr("0si");
	fprintf(stderr, "Please enter the input data for the program: ");
	while((ch = fgetc(stdin)) != EOF) {
	    prv("%dlid1+si:I", ch);
	    if (ch == '\n') {
		if (flg)
		    fprintf(stderr, ">");
		else
		    fprintf(stderr, "More? ^D to end>");
		flg = 1;
	    }
	}
	fprintf(stderr, "\n");
	pr("0li:I0sn");
	pr("[lnd1+sn;Ilp:a]si");
	do_input = 0;
    }

    if (do_input && inputmode == 3) {
	pr("[]si\n");
	do_input = 0;
    }

    if (do_input && inputmode == 4) {
	/* Simple input command '$', read a byte and push if not EOF */
	pr("[[INPUT command failed to execute.\n]P");
	prv("%dQ]sQ", max_input_depth+3);
	pr("[$z0=Qlp:a]si");
	do_input = 0;
	if (do_run) do_run = 2;
    }

    if (do_input && inputmode == 5) {
	/* New for GNU dc, character I/O with "G" ... soon after 2013! */
	fprintf(ofd, "[1G [sB_1]SA [bAla]SB 0=A Bx 0sALAaBLB+ ]si\n");
	do_input = 0;
    }

    if (do_input && inputmode == 6) {
	/* New for GNU dc, character input with "$" ... soon after 2013! */
	fprintf(ofd, "0dd:Isn\n");
	fprintf(ofd, "[[0$I0sn]SNln0;I!>N\n");
	fprintf(ofd, "[d>.ln1+dsn;I]sN_1ln0;I>N0sNLNd>.]si\n");
	do_input = 0;
    }

    if (do_input) {
	pr("[[INPUT command failed to execute.\n]P");
	prv("%dQ", max_input_depth+2);
	pr("]si");
    }

#ifndef _WIN32
    if (do_run) {
	fprintf(ofd, "LFx ");

	if (do_run>1) {
	    signal(SIGCHLD, endprog);
	    fflush(ofd);
	    while((ch = fgetc(stdin)) != EOF) {
		fprintf(ofd, "%d\n", ch);
		fflush(ofd);
	    }
	    fprintf(ofd, "_1\n");
	}
	pclose(ofd);
    } else
#endif
    if (do_trailer)
	fprintf(ofd, "LFx\n");
}

static void
print_dcstring(char * str)
{
    char buf[BUFSIZ];
    size_t outlen = 0;
    int badchar = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || badchar || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    prv("[%s]P", buf);
	    outlen = 0;
	}
	if (badchar) {
	    if (outputmode == 2)
		prv("%daP", badchar);
	    else
		prv("%dlOx", badchar);
	    do_output = 1;
	    badchar = 0;
	}
	if (!*str) break;

	if (*str > '~' || (*str < ' ' && *str != '\n' && *str != '\t')
	    || *str == '\\' /* BSD dc */
	    || (outputmode == 1 && (*str == '|' || *str == '~')) /* dc.sed */
	    || *str == '[' || *str == ']')
	    badchar = (*str & 0xFF);
	else
	    buf[outlen++] = *str;
    }
}
