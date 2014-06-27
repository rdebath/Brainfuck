#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

int bytecell = 0;
int enable_optim = 0;
int enable_be_optim = 0;
int enable_bf_optim = 0;
int enable_mov_optim = 0;
int keep_dead_code = 0;
int enable_debug;
const char * current_file;

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
static char qcmd[64];
static int qrep[64];
static int qcnt = 0;

static int madd_offset[32];
static int madd_inc[32];
static int madd_zmode[32];
static int madd_count = 0;

static int simple_mov_count = 0;

static void
outtxn(int ch, int repcnt)
{
    /* Full normal BF optimisations, hunting for MAAD loops. */
    int i, mov, inc, loopz;

    /* Sweep everything from the buffer into outopt ? */
    if ((ch != '>' && ch != '<' && ch != '+' && ch != '-' &&
	 ch != '=' && ch != '[' && ch != ']') ||
	(qcnt == 0 && ch != '[') ||
	(qcnt >= sizeof(qcmd)/sizeof(*qcmd)-1)) {

	/* If it's not a new loop add the current one too */
	if(ch != '[') {
	    qcmd[qcnt] = ch;
	    qrep[qcnt] = repcnt;
	    qcnt++;
	    ch = 0;
	}
	/* For the queue ... */
	for(i=0; i<qcnt; i++) {
	    if (qcmd[i] > 0) {
		/* Make sure counts are positive and post onward. */
		int cmd = qcmd[i];
		int rep = qrep[i];
		if (rep < 0) switch(cmd) {
		    case '>': cmd = '<'; rep = -rep; break;
		    case '<': cmd = '>'; rep = -rep; break;
		    case '+': cmd = '-'; rep = -rep; break;
		    case '-': cmd = '+'; rep = -rep; break;
		}
		outopt(cmd, rep);
	    }
	}
	qcnt = 0;
	if(ch != '[') return;
	/* We didn't add the '[' -- add it now */
    }

    /* New token */
    qcmd[qcnt] = ch;
    qrep[qcnt] = repcnt;
    qcnt++;

    /* Actually change '[-]' and '[+]' to a single token */
    if (qcnt >= 3) {
	if (qcmd[qcnt-3] == '[' && qrep[qcnt-2] == 1 && qcmd[qcnt-1] == ']'
	    && (qcmd[qcnt-2] == '-' || qcmd[qcnt-2] == '+')) {
	    qcnt -= 3;
	    outtxn('=', 0);
	    return;
	}
    }

    /* The current loop has another one embedded and it's not [-].
       Send the old loop start out */
    if (qcnt > 3 && qcmd[qcnt-3] == '[') {
	int sc[3], sr[3];
	qcnt -= 3;
	for(i=0; i<3; i++) {
	    sc[i] = qcmd[qcnt+i];
	    sr[i] = qrep[qcnt+i];
	}
	outtxn(0, 0);
	for(i=0; i<3; i++) {
	    outtxn(sc[i], sr[i]);
	}
	return;
    }

    /* Rest is to decode a 'simple' loop */
    if (ch != ']') return;

    loopz = inc = mov = 0;
    madd_count = 0;
    for(i=1; i<qcnt-1; i++) {
	int j;
	if (qcmd[i] == '>') { mov += qrep[i]; continue; }
	if (qcmd[i] == '<') { mov -= qrep[i]; continue; }
	if (qcmd[i] == '+' && mov == 0) { inc += qrep[i]; continue; }
	if (qcmd[i] == '-' && mov == 0) { inc -= qrep[i]; continue; }
	if (qcmd[i] == '=' && mov == 0) { loopz = 1; inc = qrep[i]; continue; }
	for (j=0; j<madd_count; j++) {
	    if (mov == madd_offset[j]) break;
	}
	if (j == madd_count) {
	    if (j >= sizeof(madd_offset)/sizeof(*madd_offset)) {
		/* Loop too big */
		outtxn(0,0);
		return;
	    }
	    madd_offset[madd_count] = mov;
	    madd_inc[madd_count] = 0;
	    madd_zmode[madd_count] = 0;
	    madd_count ++;
	}
	if (qcmd[i] == '=') {
	    madd_zmode[j] = 1;
	    madd_inc[j] = qrep[i];
	    continue;
	}
	if (qcmd[i] == '+') { madd_inc[j] += qrep[i]; continue; }
	if (qcmd[i] == '-') { madd_inc[j] -= qrep[i]; continue; }

	/* WTF! Some unknown command has made it here!? */
	outtxn(0,0);
	return;
    }
    /* Last check for not simple. */
    if (mov
	|| (loopz == 0 && inc != -1 && inc != 1)
	|| (loopz != 0 && inc != 0)
	|| (!enable_be_optim && loopz) )
	{ outtxn(0, 0); return; }

    /* Delete old loop */
    qcnt = 0;

    /* Start new */
    qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
    if (loopz) {
	inc = -1;
	qcmd[qcnt] = 'Q'; qrep[qcnt] = 1; qcnt ++;
	qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
    }
    if (enable_be_optim) {
	qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;
    }

    for (i=0; i<madd_count; i++) {
	if (!madd_zmode[i] && madd_inc[i] == 0) continue; /* NOP */
	if (mov != madd_offset[i]) {
	    qcmd[qcnt] = '>'; qrep[qcnt] = madd_offset[i] - mov; qcnt ++;
	    mov = madd_offset[i];
	}
	if (madd_zmode[i]) {
	    qcmd[qcnt] = 'Q'; qrep[qcnt] = madd_inc[i]; qcnt ++;
	} else {
	    qcmd[qcnt] = 'M'; qrep[qcnt] = madd_inc[i] * -inc;
	    if (qrep[qcnt] == 1) qcmd[qcnt] = 'S';
	    if (qrep[qcnt] < 0) {
		qcmd[qcnt] = 'N';
		qrep[qcnt] = -qrep[qcnt];
	    }

	    /*
	     * Note the 'BOFF' define; this is an optimisation
	     * tweak that allows loops, that may be skipped over, to
	     * be converted into move/add instructions even if the
	     * references inside the loop may be converted so they
	     * 'Add zero' to cells before the start of the tape. The
	     * start of the tape will be shifted by this count so the
	     * 'add zero' doesn't cause a segfault.
	     *
	     * Any reference that is further back than this range
	     * cannot be proven to be after the physical start of
	     * the tape so it must keep the 'if non-zero' condition
	     * to avert the possible segfault.
	     */

	    if (mov < -BOFF) qcmd[qcnt] = qcmd[qcnt] - 'A' + 'a';
	    qcnt++;
	}
    }

    if (mov != 0) {
	qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
    }
    if (!enable_be_optim) {
	qcmd[qcnt] = 'N'; qrep[qcnt] = 1; qcnt ++;
	qcmd[qcnt] = 'E'; qrep[qcnt] = 0; qcnt ++;
    }

    /* And forward the converted loop */
    outtxn(0, 0);
}

/*
    For full structual optimisation (loops to multiplies) this function
    is skipped and the the outtxn function is called.

    Otherwise there is a simple filter to find [-]+++ sequences for
    "-Obf" optimisation, it ignores '[+]'.  The symbols are then passed
    to the constant folding optimisations, that can now output pure BF
    for any input.

    The third option "-Omov" is a filter that just merges '<>' commands
    and sends them directly to the BE.

    The last is no transformations, just pass to the BE.
*/
void
outrun(int ch, int count)
{
static int zstate = 0;
    if (enable_optim) {
	outtxn(ch, count);

    } else if (enable_bf_optim) {
	switch(zstate)
	{
	case 1:
	    if (count == 1 && ch == '-') { zstate++; return; }
	    outopt('[', 1);
	    break;
	case 2:
	    if (count == 1 && ch == ']') { zstate++; return; }
	    outopt('[', 1);
	    outopt('-', 1);
	    break;
	case 3:
	    if (ch == '+') { outopt('=', count); zstate=0; return; }
	    if (ch == '-') { outopt('=', -count); zstate=0; return; }
	    outopt('=', 0);
	    break;
	}
	zstate=0;
	if (count == 1 && ch == '[') { zstate++; return; }
	outopt(ch, count);

    } else if (enable_mov_optim) {
	/* This is a very simple optimisation of pointer movements, it's
	 * normally not needed as it's a side effect of the other types
	 * of optimisation.
	 */
	if (ch == '>') { simple_mov_count += count; return; }
	if (ch == '<') { simple_mov_count -= count; return; }
	if (simple_mov_count > 0) outcmd('>', simple_mov_count);
	if (simple_mov_count < 0) outcmd('<', -simple_mov_count);
	simple_mov_count = 0;
	outcmd(ch, count);
	return;

    } else {
	/* Disable optimisations; input codes are sent directly to BE */
	outcmd(ch, count);
	return;
    }
}

/*
 *  Decode arguments for the FE
 */
int byte_supported, nobyte_supported;
int enable_rle = 0;

int
check_argv(const char * arg)
{
    if (byte_supported && strcmp(arg, "-b") == 0) {
	check_arg(arg);
	bytecell++;
    } else if (nobyte_supported && strcmp(arg, "-no-byte") == 0) {
	check_arg(arg);
	bytecell=0;

    } else if (strcmp(arg, "-m") == 0) {
	check_arg(arg);
	enable_optim = 0;
	enable_be_optim = 0;
	keep_dead_code = 1;
    } else if (strcmp(arg, "-O") == 0) {
	enable_optim = 1;
	check_arg(arg);
    } else if (strcmp(arg, "-Obf") == 0) {
	enable_bf_optim=1;
	enable_optim = 0;
    } else if (strcmp(arg, "-Omov") == 0) {
	enable_mov_optim=1;
	enable_optim = 0;

    } else if (strcmp(arg, "-#") == 0 && check_arg(arg)) {
	enable_debug++;
    } else if (strcmp(arg, "-R") == 0) {
	enable_rle++;

    } else if (strcmp(arg, "-h") == 0) {
	return 0;
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
    int ch, lastch=']', c=0, m, b=0, lc=0, ar, cf=0;
    FILE * ifd;
    int digits = 0, number = 0, multi = 1;
    int qstring = 0;

    byte_supported = check_arg("-b");
    nobyte_supported = check_arg("-no-byte");
    if (!byte_supported && !nobyte_supported)
	nobyte_supported = byte_supported = 1;
    bytecell = !nobyte_supported;
    enable_optim = enable_be_optim = check_arg("-O");

    for(;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;

	} else if (strcmp(argv[1], "-h") == 0) {

	    fprintf(stderr, "%s: [options] [File]\n", pgm);
	    fprintf(stderr, "%s\n",
	    "\t"    "-h      This message"
	    "\n\t"  "-b      Force byte cells"
	    "\n\t"  "-#      Turn on trace code."
	    "\n\t"  "-R      Decode rle on '+-<>', quoted strings and '='."
	    "\n\t"  "-m      Disable optimisation (including dead loop removal)"
	    "\n\t"  "-O      Enable full optimisation"
	    "\n\t"  "-Obf    Enable simple optimisation suitable for BF output"
	    "\n\t"  "-Omov   Enable only movement optimisation"
	    );

	    check_arg(argv[1]);
	    exit(0);
	} else if (check_argv(argv[1])) {
	    argc--; argv++;
	} else if (strcmp(argv[1], "--") == 0) {
	    argc--; argv++;
	    break;
	} else if (argv[1][0] == '-') {
	    char * ap = argv[1]+1;
	    char buf[4] = "-X";
	    while(*ap) {
		buf[1] = *ap;
		if (!check_argv(buf))
		    break;
		ap++;
	    }
	    if (*ap) {
		fprintf(stderr, "Unknown option '%s'; try -h for option list\n",
			argv[1]);
		exit(1);
	    }
	    argc--; argv++;
	} else break;
    }

    outrun('!', 0);
    for(ar=0+(argc>1); ar<argc; ar++) {
	if (argc<=1 || strcmp(argv[ar], "-") == 0) {
	    ifd = stdin;
	    current_file = "stdin";
	} else if((ifd = fopen(argv[ar],"r")) == 0) {
	    perror(argv[ar]); exit(1);
	} else
	    current_file = argv[ar];

	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' ||
		qstring || lc || b || (c==0 && cf==0))) {
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
		    outrun('[', 1); outrun('-', 1); outrun(']', 1);
		    outrun('+', ch); outrun('.', 1); lastch = '.';
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
		((ch != '"' && ch != '=') || !enable_rle)) continue;
	    /* Check for loop comments; ie: ][ comment ] */
	    if (lc || (ch=='[' && lastch==']' && !keep_dead_code)) {
		lc += (ch=='[') - (ch==']'); continue;
	    }
	    if (lc) continue;
	    cf=1;
	    /* Do the RLE */
	    if (m && ch == lastch) { c+=multi; continue; }
	    /* Post the RLE token onward */
	    if (c) outrun(lastch, c);
	    if (!m) {
		/* Non RLE tokens here */
		if (ch == '"') { qstring++; continue; }
		if (ch == '=') {
		    outrun('[', 1); outrun('-', 1); outrun(']', 1);
		    lastch = ']';
		    continue;
		}
		if (!b && ch == ']') continue; /* Ignore too many ']' */
		b += (ch=='[') - (ch==']');
		if (lastch == '[' && ch == ']') outrun('X', 1);
		outrun(ch, 1);
		c = 0;
	    } else
		c = multi;
	    lastch = ch;
	}
	if (ifd != stdin) fclose(ifd);
    }
    if(c) outrun(lastch, c);
    while(b>0){ outrun(']', 1); b--;} /* Not enough ']', add some. */
    if (enable_debug) outrun('#', 0);
    outrun('~', 0);
    return 0;
}
