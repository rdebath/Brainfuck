#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

int bytecell = 0;
int enable_optim = 0;
int enable_bf_optim = 0;
char * current_file;
int enable_debug;

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
 */
static char qcmd[64];
static int qrep[64];
static int qcnt = 0;

static int madd_offset[32];
static int madd_inc[32];
static int madd_zmode[32];
static int madd_count = 0;

static void
outrun(int ch, int repcnt)
{
    if (!enable_optim) {
	if (!enable_bf_optim)
	    outcmd(ch, repcnt);
	else
	    outopt(ch, repcnt);
    } else {
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
		outrun('=', 0);
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
	    outrun(0, 0);
	    for(i=0; i<3; i++) {
		outrun(sc[i], sr[i]);
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
		    outrun(0,0);
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
	    outrun(0,0);
	    return;
	}
	/* Last check for not simple. */
	if (mov
	    || (loopz == 0 && inc != -1 && inc != 1)
	    || (loopz != 0 && inc != 0) )
	    { outrun(0, 0); return; }

	/* Delete old loop */
	qcnt = 0;

	/* Start new */
	if (loopz) {
	    /* *m = (*m != 0) */
	    qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
	    qcmd[qcnt] = 'Q'; qrep[qcnt] = 1; qcnt ++;
	    inc = -1;
	}
	qcmd[qcnt] = 'B'; qrep[qcnt] = 1; qcnt ++;
	qcmd[qcnt] = '='; qrep[qcnt] = 0; qcnt ++;
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

		if (mov < -BOFF) qcmd[qcnt] = qcmd[qcnt] - 'A' + 'a';
		qcnt++;
	    }
	}

	if (mov != 0) {
	    qcmd[qcnt] = '>'; qrep[qcnt] = 0 - mov; qcnt ++;
	}

	/* And forward the converted loop */
	outrun(0, 0);
    }
}

int
main(int argc, char ** argv)
{
    char * pgm = argv[0];
    int ch, lastch=']', c=0, m, b=0, lc=0;
    FILE * ifd;
    int opt_supported = -1, byte_supported, nobyte_supported;

    byte_supported = check_arg("-b");
    nobyte_supported = check_arg("-no-byte");
    if (!byte_supported && !nobyte_supported)
	nobyte_supported = byte_supported = 1;
    bytecell = !nobyte_supported;

    for(;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;

	} else if (byte_supported && strcmp(argv[1], "-b") == 0) {
	    check_arg(argv[1]);
	    bytecell++; argc--; argv++;
	} else if (nobyte_supported && strcmp(argv[1], "-no-byte") == 0) {
	    check_arg(argv[1]);
	    bytecell=0; argc--; argv++;

	} else if (strcmp(argv[1], "-m") == 0 && check_arg("-O")) {
	    if (opt_supported == -1)
		opt_supported = enable_optim = check_arg("-O");
	    check_arg(argv[1]);
	    enable_optim=0; argc--; argv++;
	} else if (strcmp(argv[1], "-O") == 0 && check_arg("-O")) {
	    opt_supported = enable_optim = check_arg(argv[1]);
	    argc--; argv++;
	} else if (strcmp(argv[1], "-Obf") == 0) {
	    enable_bf_optim=1; argc--; argv++;

	} else if (strcmp(argv[1], "-#") == 0 && check_arg(argv[1])) {
	    enable_debug++; argc--; argv++;
	} else if (strcmp(argv[1], "-h") == 0) {

	    fprintf(stderr, "%s: [options] [File]\n", pgm);
	    fprintf(stderr, "%s\n",
	    "\t"    "-h      This message"
	    "\n\t"  "-b      Force byte cells"
	    "\n\t"  "-#      Turn on trace code.");
	    if (check_arg("-O"))
		fprintf(stderr, "%s\n",
		"\t"    "-O      Enable optimisation"
		"\n\t"  "-m      Disable optimisation");

	    check_arg(argv[1]);
	    exit(0);

	} else if (check_arg(argv[1])) {
	    argc--; argv++;
	} else if (strcmp(argv[1], "--") == 0) {
	    argc--; argv++;
	    break;
	} else if (argv[1][0] == '-') {
	    fprintf(stderr, "Unknown option '%s'; try -h for option list\n",
		    argv[1]);
	    exit(1);
	} else break;
    }

    if (opt_supported == -1)
	opt_supported = enable_optim = check_arg("-O");

    if (argc<=1 || strcmp(argv[1], "-") == 0) {
	ifd = stdin;
	current_file = "stdin";
    } else if((ifd = fopen(argv[1],"r")) == 0) {
	perror(argv[1]); exit(1);
    } else
	current_file = argv[1];
    outrun('!', 0);
    while((ch = fgetc(ifd)) != EOF && (ifd!=stdin || ch != '!')) {
	/* These chars are RLE */
	m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	/* These ones are not */
	if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',' &&
	    (ch != '#' || !enable_debug)) continue;
	/* Check for loop comments; ie: ][ comment ] */
	if (lc || (ch=='[' && lastch==']')) { lc += (ch=='[') - (ch==']'); continue; }
	if (lc) continue;
	/* Do the RLE */
	if (m && ch == lastch) { c++; continue; }
	/* Post the RLE token onward */
	if (c) outrun(lastch, c);
	c = m;
	if (!m) {
	    /* Non RLE tokens here */
	    if (!b && ch == ']') continue; /* Ignore too many ']' */
	    b += (ch=='[') - (ch==']');
	    if (lastch == '[' && ch == ']') outrun('X', 1);
	    outrun(ch, 1);
	}
	lastch = ch;
    }
    if (ifd != stdin) fclose(ifd);
    if(c) outrun(lastch, c);
    while(b>0){ outrun(']', 1); b--;} /* Not enough ']', add some. */
    if (enable_debug) outrun('#', 0);
    outrun('~', 0);
    return 0;
}
