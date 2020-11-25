#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bf2any_ex.h"

#ifndef TAPELEN
#define TAPELEN 0x100000
#endif

#ifndef BOFF
#define BOFF 256
#endif

static int opt_optim = 0;
int disable_init_optim = 0;
static const char * current_file;

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

static void outrun(int ch, int count);
static void load_and_run(char ** filelist, int filecount);
static void pipe_to_be(char ** filelist, int filecount);

/*
 *  Decode arguments.
 */
static int enable_rle = 0;
static int backend_only = 0;
static int eofcell = 0;
static int tapelen = TAPELEN;

static int
check_arg(const char * arg) {
    if (be_interface.check_arg == 0) return 0;
    return (*be_interface.check_arg)(arg);
}

static int
check_argv(const char * arg)
{
    if (strcmp(arg, "-b") == 0) {
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
    } else if (strcmp(arg, "-be-pipe") == 0) {
	if (be_interface.disable_be_optim) return 0;
	backend_only = 1;
	enable_rle++;
	enable_debug = 1;

    } else if (strcmp(arg, "-n") == 0) {
	eofcell = 0;
    } else if (strcmp(arg, "-z") == 0) {
	eofcell = 1;
    } else if (strcmp(arg, "-e") == 0) {
	eofcell = 2;

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
    int ar, mm=0;
    char * pgm = argv[0];
    char ** filelist = 0;
    int filecount = 0;

    tapesz = tapeinit + tapelen;
    filelist = calloc(argc, sizeof*filelist);

    if (be_interface.gen_code == 0) {
	fprintf(stderr, "Compiler failure\n");
	exit(255);
    }

    for(ar=1;ar<argc;ar++) {
	if (argv[ar][0] != '-' || argv[ar][1] == '\0' || mm) {
	    filelist[filecount++] = argv[ar];

	} else if (strcmp(argv[ar], "-h") == 0 || strcmp(argv[ar], "--help") == 0) {

	    fprintf(stderr, "%s: [options] [File]\n", pgm);
	    fprintf(stderr, "%s\n",
		"\t"    "-h      This message");

	    if (!be_interface.bytesonly)
		fprintf(stderr, "%s\n",
		    "\t"    "-b      Force byte cells"
		    );

	    if (be_interface.hasdebug)
		fprintf(stderr, "%s\n",
		    "\t"    "-#      Turn on trace code."
		    );

	    fprintf(stderr, "%s%d%s\n",
	    "\t"    "-R      Decode rle on '+-<>', quoted strings and '='."
	    "\n\t"  "-m      Disable optimisation (including dead loop removal)"
	    "\n\t"  "-p      Optimise as part of a BF program"
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
	    mm = 1;
	} else {
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
	}
    }

    check_arg("+init");	/* Callout to BE for soft init. */

    /* Defaults if not told */
    if (!opt_optim && be_interface.disable_fe_optim)
	opt_optim = disable_init_optim = 1;

    if (!opt_optim)
	opt_optim = enable_optim = 1;

    tapeinit = (!be_interface.disable_be_optim && !backend_only)?BOFF:0;
    tapesz = tapeinit + tapelen;

    if (be_interface.bytesonly) bytecell = 1;

    if (filecount == 0)
	filelist[filecount++] = "-";

    if (backend_only)
	pipe_to_be(filelist, filecount);
    else
	load_and_run(filelist, filecount);
    return 0;
}

void
load_and_run(char ** filelist, int filecount)
{
    FILE * ifd;
    int ch, lastch=']', c=0, m, b=0, lc=0, ar, inp=0;
    int digits = 0, number = 0, ov_flg = 0, multi = 1;
    int qstring = 0;
    char * xc = 0;

    if (disable_init_optim)
	lastch = 0;

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
		number = ov_iadd(ov_imul(number, 10, &ov_flg), ch-'0', &ov_flg);
		continue;
	    }
	    if (ch == ' ' || ch == '\t') continue;
	    if (!digits || ov_flg) number = 1;
	    /*GCC*/ digits=ov_flg=0;
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
	    if (m && ch == lastch) {
		int ov=0, res;
		res = ov_iadd(c, multi, &ov);
		if (!ov) { c = res; continue; }
	    }
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
		if (ch == ',') {
		    inp = 1;
		    if (eofcell == 1) outrun('=', 0);
		    if (eofcell == 2) outrun('=', -1);
		}
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
}

void
pipe_to_be(char ** filelist, int filecount)
{
    FILE * ifd;
    int ar, hashwarn = 0;

    for(ar=0; ar<filecount; ar++) {
	int ch, m, m0, inp=0, cmt=0, di = 0;
	int digits = 0, number = 0, ov_flg = 0, multi = 1;
	int qstring = 0;

	if (strcmp(filelist[ar], "-") == 0) {
	    ifd = stdin;
	    current_file = "stdin";
	} else if((ifd = fopen(filelist[ar],"r")) == 0) {
	    perror(filelist[ar]); exit(1);
	} else
	    current_file = filelist[ar];

	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' ||
		!inp || qstring)) {
	    /* Quoted strings are printed; do NOT set current cell) */
	    if (qstring) {
		if (ch == '"') {
		    qstring++;
		    if (qstring == 2) continue;
		    if (qstring == 3) qstring = 1;
		}
		if (qstring == 2) {
		    qstring = 0;
		    flush_string();
		} else {
		    add_string(ch);
		    continue;
		}
	    }

	    /* Comments */
	    if (cmt || ch == '{') { cmt += (ch=='{') - (ch=='}'); continue; }

	    /* Source RLE decoding */
	    if (ch >= '0' && ch <= '9') {
		digits = 1;
		number = ov_iadd(ov_imul(number, 10, &ov_flg), ch-'0', &ov_flg);
		continue;
	    }
	    if (ch == '~' && digits) {
		number = ov_ineg(number, &ov_flg);
		number = ov_iadd(number, -1, &ov_flg);
		continue;
	    }
	    if (ch == ' ' || ch == '\t') continue;
	    if ( ov_flg ) number=digits=ov_flg=0;

	    if (!di) {
		if (ch == '\n') continue;
		if (ch == '!' ) {
		    /* Arg values
		     *	>0 Compiled for specific bit size
		     *	=0 Compiled for any bit size >= 8 bits. (default)
		     *  <0 Compiled for any bit size >= -N bits.
		     */

		    if (number == 8)
			bytecell = 1;
		    else if (number == (int)sizeof(int)*CHAR_BIT &&
			    be_interface.cells_are_ints)
			bytecell = 0;
		    else if (number < -8 && !be_interface.bytesonly)
			bytecell = 0;
		    else if (number == 0 || number == -8)
			;
		    else {
			fprintf(stderr, "Bitsize of %d not supported for this backend.\n", number);
			exit(1);
		    }
		    number = 0; digits = 0;
		    continue;
		}
		if (ch == '>' && tapeinit <= 0) {
		    tapeinit = number + (digits==0);
		    tapesz = tapeinit + tapelen;
		    number = 0; digits = 0;
		    continue;
		}
		di = 1;
		outcmd('!', 0);
	    }

	    /* These chars have an argument. */
	    m = (ch == '>' || ch == '<' || ch == '+' || ch == '-' ||
		 ch == '=' || ch == 'N' || ch == 'M' || ch == 'C' ||
		 ch == 'D');

	    /* These ones do not */
	    m0 = (ch == '[' || ch == ']' || ch == '.' || ch == ',' ||
		  ch == 'I' || ch == 'E' || ch == 'B' || ch == 'S' ||
		  ch == 'T' || ch == 'V' || ch == 'W' || ch == '"' ||
		  ch == 'X' || ch == '*' || ch == '/' || ch == '%');

	    if (!m) {
		multi = 0;
	    } else if (ch == '=') {
		if (!digits) number = 0;
		multi = number;
	    } else {
		if (!digits) number = 1;
		multi = number;
	    }

	    number = 0; digits = 0;

	    if (ch == '\n' || ch == '\f' || ch == '\a') continue;

	    if(!m && !m0) {
		if (ch == '#') {
		    if (!hashwarn)
			fprintf(stderr,
			    "WARNING: Command '#' ignored by backend.\n");
		    hashwarn = 1;
		} else {
		    if (ch > ' ' && ch <= '~')
			fprintf(stderr, "Command '%c' not supported\n", ch);
		    else
			fprintf(stderr, "Command 0x%02x not supported\n", ch);
		    exit(97);
		}
	    }

	    if (ch == '"') { qstring++; continue; }
	    if (ch == ',') inp = 1;

	    outcmd(ch, multi);
	}
	if (ifd != stdin) fclose(ifd);
    }
    outcmd('~', 0);
}

void
outcmd(int ch, int count)
{
    (*be_interface.gen_code)(ch, count, 0);
}

/*
    For full structual optimisation (loops to multiplies) this function
    calls the outtxn function after finding '=' commands otherwise the
    commands are just pass to the BE.
*/
static void
out2opt(int ch, int count)
{
    if (!enable_optim) outcmd(ch, count);
    else outtxn(ch, count);
}

static void
outrun(int ch, int count)
{
static int zstate = 0;
static int icount = 0;
    switch(zstate)
    {
    case 1:
	if (count%2 == 1 && enable_optim) {
	    if (ch == '-') { zstate=2; icount = count; return; }
	    if (ch == '+') { zstate=3; icount = count; return; }
	}
	if (count == 1 && ch == '-') { zstate=2; icount = count; return; }
	out2opt('[', 0);
	break;
    case 2:
	if (ch == ']') { zstate=4; return; }
	out2opt('[', 0);
	out2opt('-', icount);
	break;
    case 3:
	if (ch == ']') { zstate=4; return; }
	out2opt('[', 0);
	out2opt('+', icount);
	break;
    case 4:
	if (ch == '+') { out2opt('=', count); zstate=0; return; }
	if (ch == '-') { out2opt('=', -count); zstate=0; return; }
	out2opt('=', 0);
	break;
    }
    zstate=0;
    if (ch == '[') { zstate++; return; }
    out2opt(ch, count);
}
