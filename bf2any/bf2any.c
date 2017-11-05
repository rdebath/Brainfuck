#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TAPELEN
#define TAPELEN 0x100000
#endif

#include "bf2any.h"
#include "bf2loop.h"

int opt_cellsize = 0;
int tapelen = TAPELEN;
int opt_optim = 0;
int enable_optim = 0;
int disable_init_optim = 0;
int enable_debug;
const char * current_file;
char * extra_commands = 0;

/* Brainfuck loader.
 *
 * This is an "optimising" BF loader it does simple translations of BF
 * instructions into tokens.
 *
 * The peephole optimisations it does are:
 *
 * RLE of '+', '-', '>', '<'.
 * Remove loop comments; ie: ][ comment ]
 * Change '[-]' and '[+]' to single instructions.
 * Simple "Move or ADD" ( MADD ) loops containing a plain decrement of
 * the loop counter and '+', '-' or '[-]' on other cells.
 * Strings like '[-]+++++++' are changed to a set token.
 * The linked bf2const.c then buffers token streams without loops and reorders
 * them into groups of single sets or adds. It also notices with an output is
 * given a constant and passes this on as a special token.
 *
 * After all this the 'run' backend will check for '[<<]' and '[-<<]' loops an
 * convert those into special tokens.
 *
 * In addition it marks the trivial infinite loop '[]' as '[X]', this tends
 * to be useful for languages that hate empty loops like Pascal and Python.
 * Lastly it makes sure that brackets are balanced by removing or adding
 * extra ']' tokens.
 *
 * The tokens are passed to the backend for conversion into the final code.
 */

void outrun(int ch, int count);

/*
 *  Decode arguments.
 */
int enable_rle = 0;

int
check_arg(const char * arg) {
    if (be_interface.check_arg == 0) return 0;
    return (*be_interface.check_arg)(arg);
}

int
check_argv(const char * arg)
{
    if (bytecell >= 0 && !nobytecell && strcmp(arg, "-b") == 0) {
	bytecell = 1;

    } else if (strcmp(arg, "-m") == 0) {
	check_arg(arg);
	opt_optim = disable_init_optim = 1;
    } else if (strcmp(arg, "-O") == 0) {
	opt_optim = 1;
	enable_optim = 1;
    } else if (strcmp(arg, "-p") == 0) {
	disable_init_optim = 1;

    } else if (strcmp(arg, "-#") == 0 && check_arg(arg)) {
	enable_debug++;
    } else if (strcmp(arg, "-R") == 0) {
	enable_rle++;

    } else if (strcmp(arg, "-h") == 0) {
	return 0;
    } else if (strncmp(arg, "-M", 2) == 0 && arg[2] != 0) {
	tapelen = strtoul(arg+2, 0, 10);
	if (tapelen < 1) tapelen = TAPELEN;
	return 1;
    } else if (check_arg(arg)) {
	;
    }
    else return 0;
    return 1;
}

/*
 * This main function is quite high density, it's functions are:
 *  1) Configure the FE with the BE capabilities.
 *  2) Despatch arguments to FE and BE argument checkers.
 *  3) Open and read the BF file.
 *  4) Decode the BF into calls of the outrun() function.
 *  5) Run length encode calls to the outrun() function.
 *  6) Ensure that '[', ']' commands balance.
 *  7) Allow the '#' command only if enabled.
 *  8) If enabled; decode input RLE (a number prefix on the "+-<>" commands)
 *  9) If enabled; decode input quoted strings as BF print sequences.
 * 10) If enabled; convert the '=' command into '[-]'.
 */
int
main(int argc, char ** argv)
{
    char * pgm = argv[0];
    int ch, lastch=']', c=0, m, b=0, lc=0, ar, inp=0;
    FILE * ifd;
    int digits = 0, number = 0, multi = 1;
    int qstring = 0;
    char * xc = 0;
    char ** filelist = 0;
    int filecount = 0;

    filelist = calloc(argc, sizeof*filelist);

    for(ar=1;ar<argc;ar++) {
	if (argv[ar][0] != '-' || argv[ar][1] == '\0') {
	    filelist[filecount++] = argv[ar];

	} else if (strcmp(argv[ar], "-h") == 0) {

	    fprintf(stderr, "%s: [options] [File]\n", pgm);
	    fprintf(stderr, "%s%d%s\n",
	    "\t"    "-h      This message"
	    "\n\t"  "-b      Force byte cells"
	    "\n\t"  "-#      Turn on trace code."
	    "\n\t"  "-R      Decode rle on '+-<>', quoted strings and '='."
	    "\n\t"  "-m      Disable optimisation (including dead loop removal)"
	    "\n\t"  "-p      This is a part of a BF program"
	    "\n\t"  "-O      Enable optimisation"
	    "\n\t"  "-M<num> Set length of tape, default is ", TAPELEN,
	    ""
	    );

	    if (check_arg("-M"))
		printf("\t-M      Set the tape to dynamic\n");

	    check_arg(argv[ar]);
	    exit(0);
	} else if (check_argv(argv[ar])) {
	    ;
	} else if (strcmp(argv[ar], "--") == 0) {
	    ;
	    break;
	} else if (argv[ar][0] == '-') {
	    char * ap = argv[ar]+1;
	    static char buf[4] = "-X";
	    while(*ap) {
		buf[1] = *ap;
		if (!check_argv(buf))
		    break;
		ap++;
	    }
	    if (*ap) {
		fprintf(stderr, "Unknown option '%s'; try -h for option list\n",
			argv[ar]);
		exit(1);
	    }
	} else
	    filelist[filecount++] = argv[ar];
    }

    if (check_arg("+init")) {	/* For bf2bf to choose optimisation method */
	if (!opt_optim && check_arg("+no-rle")) {
	    opt_optim = disable_init_optim = 1;
	}
    }

    /* Defaults if not told */
    if (!opt_optim)
	opt_optim = enable_optim = 1;

    if (disable_init_optim)
	lastch = 0;

    if (bytecell != 0) bytecell = 1;

    if (filecount == 0)
	filelist[filecount++] = "-";

    /* Now we do it ... */
    outrun('!', 0);
    for(ar=0; ar<filecount; ar++) {

	if (strcmp(filelist[ar], "-") == 0) {
	    ifd = stdin;
	    current_file = "stdin";
	} else if((ifd = fopen(filelist[ar],"r")) == 0) {
	    perror(filelist[ar]); exit(1);
	} else
	    current_file = filelist[ar];

	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' ||
		b || !inp || lc || qstring)) {
	    /* Quoted strings are printed. (And set current cell) */
	    if (qstring) {
		if (ch == '"') {
		    qstring++;
		    if (qstring == 2) continue;
		    if (qstring == 3) qstring = 1;
		}
		if (qstring == 2) {
		    qstring = 0;
		} else {
		    outrun('=', ch); outrun('.', 0);
		    lastch = '.'; c = 0;
		    continue;
		}
	    }
	    /* Source RLE decoding */
	    if (ch >= '0' && ch <= '9') {
		digits = 1;
		number = number*10 + ch-'0';
		continue;
	    }
	    if (ch == ' ' || ch == '\t') continue;
	    if (!digits) number = 1; digits=0;
	    multi = enable_rle?number:1; number = 0;
	    /* These chars are RLE */
	    m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	    /* These ones are not */
	    if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',' &&
		(ch != '#' || !enable_debug) &&
		(!extra_commands || (xc = strchr(extra_commands, ch)) == 0) &&
		((ch != '"' && ch != '=') || !enable_rle)) continue;
	    /* Check for loop comments; ie: ][ comment ] */
	    if (lc || (ch=='[' && lastch==']' && enable_optim)) {
		lc += (ch=='[') - (ch==']'); continue;
	    }
	    if (lc) continue;
	    /* Do the RLE */
	    if (m && ch == lastch) { c+=multi; continue; }
	    /* Post the RLE token onward */
	    if (c) { outrun(lastch, c); c=0; }
	    if (!m) {
		/* Non RLE tokens here */
		if (xc) {
		    outrun(256+(xc-extra_commands), 0);
		    xc = 0;
		    continue;
		}
		if (ch == '"') { qstring++; continue; }
		if (ch == ',') inp = 1;
		if (!b && ch == ']') {
		    fprintf(stderr, "Warning: skipping unbalanced ']' command.\n");
		    continue; /* Ignore too many ']' */
		}
		b += (ch=='[') - (ch==']');
		if (lastch == '[' && ch == ']') outrun('X', 1);
		outrun(ch, 0);
	    } else
		c = multi;
	    lastch = ch;
	}
	if (ifd != stdin) fclose(ifd);
    }
    if(c) outrun(lastch, c);
    if(b>0) {
	fprintf(stderr, "Warning: closing unbalanced '[' command.\n");
	outrun('=', 0);
	while(b>0){ outrun(']', 0); b--;} /* Not enough ']', add some. */
    }
    if (enable_debug && lastch != '#') outrun('#', 0);
    outrun('~', 0);
    return 0;
}

/*
    For full structual optimisation (loops to multiplies) this function
    calls the outtxn function after finding '=' commands otherwise the
    commands are just pass to the BE.
*/
void
outrun(int ch, int count)
{
static int zstate = 0;
static int icount = 0;
    if (!enable_optim) { outcmd(ch, count); return; }

    switch(zstate)
    {
    case 1:
	if (count%2 == 1 && ch == '-') { zstate=2; icount = count; return; }
	if (count%2 == 1 && ch == '+') { zstate=3; icount = count; return; }
	outtxn('[', 0);
	break;
    case 2:
	if (ch == ']') { zstate=4; return; }
	outtxn('[', 0);
	outtxn('-', icount);
	break;
    case 3:
	if (ch == ']') { zstate=4; return; }
	outtxn('[', 0);
	outtxn('+', icount);
	break;
    case 4:
	if (ch == '+') { outtxn('=', count); zstate=0; return; }
	if (ch == '-') { outtxn('=', -count); zstate=0; return; }
	outtxn('=', 0);
	break;
    }
    zstate=0;
    if (ch == '[') { zstate++; return; }
    outtxn(ch, count);
}
