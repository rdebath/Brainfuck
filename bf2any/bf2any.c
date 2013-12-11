#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bytecell = 0;
static int enable_optim;

void outcmd(int ch, int count);
int check_arg(char * arg);

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
 *
 * In addition it marks the trivial infinite loop '[]' as '[X]', this tends
 * to be useful for languages that hate empty loops like Pascal and Python.
 * Lastly it makes sure that brackets are balanced by removing or adding
 * extra ']' tokens.
 *
 * TODO? Sort all changes within a run of "-+<>" characters, remove NOPs.
 *       Similar for ".," too ?
 *       Flatten constant loops to large sets or adds ?
 */
static char qcmd[32];
static int qrep[32];
static int qcnt = 0;
static int curmov = 0;
static int saved_zset = 0;

static int madd_offset[32];
static int madd_inc[32];
static int madd_zmode[32];
static int madd_count = 0;

static void
outrun(int ch, int repcnt)
{
    if (!enable_optim)
	outcmd(ch, repcnt);
    else {
	int i, mov, inc;

	/* Sweep everything from the buffer into outcmd ? */
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
		    /* Merge a [-] with a following ++++ */
		    if (saved_zset) {
			saved_zset = 0;
			if (qcmd[i] == '+') {
			    outcmd('=', qrep[i]);
			    continue;
			} else
			    outcmd('=', 0);
		    }
		    /* Delay < and > to merge movements round deleted tokens */
		    if (qcmd[i] == '>') { curmov += qrep[i]; continue; }
		    if (qcmd[i] == '<') { curmov -= qrep[i]; continue; }
		    /* send the delayed <> strings */
		    if (curmov > 0) outcmd('>', curmov);
		    if (curmov < 0) outcmd('<', -curmov);
		    curmov = 0;

		    if (qcmd[i] == '=' && qrep[i] == 0)
			/* Delay a [-] to see if we have any +++ after */
			saved_zset++;
		    else
			outcmd(qcmd[i], qrep[i]);
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

	/* Need to pad the [] loop, also the 'X' can be an error message. */
	if (qcnt >= 2) {
	    if (qcmd[qcnt-2] == '[' && qcmd[qcnt-1] == ']') {
		qcnt -= 2;
		outrun('[', 1);
		outrun('X', 1);
		outrun(']', 1);
		return;
	    }
	}

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

	inc = mov = 0;
	madd_count = 0;
	for(i=1; i<qcnt-1; i++) {
	    int j;
	    if (qcmd[i] == '>') { mov += qrep[i]; continue; }
	    if (qcmd[i] == '<') { mov -= qrep[i]; continue; }
	    if (qcmd[i] == '+' && mov == 0) { inc += qrep[i]; continue; }
	    if (qcmd[i] == '-' && mov == 0) { inc -= qrep[i]; continue; }
	    /* It may be an if, but that's not interesting now */
	    if (qcmd[i] == '=' && mov == 0) { outrun(0, 0); return; }
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
	if (mov || (inc != -1 && inc != 1)) { outrun(0, 0); return; }

	/* Delete old loop */
	qcnt = 0;

	/* Start new */
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
    int ch, lastch=']', c=0, m, b=0, lc=0;
    FILE * ifd;
    enable_optim = check_arg("-O");
    for(;;) {
	if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	    break;
	} else if (strcmp(argv[1], "-b") == 0) {
	    bytecell++; argc--; argv++;
	} else if (enable_optim && strcmp(argv[1], "-m") == 0) {
	    enable_optim=0; argc--; argv++;
	} else if (strcmp(argv[1], "-O") == 0 && check_arg(argv[1])) {
	    enable_optim=1; argc--; argv++;
	} else if (check_arg(argv[1])) {
	    argc--; argv++;
	} else if (strcmp(argv[1], "--") == 0) {
	    argc--; argv++;
	    break;
	} else if (argv[1][0] == '-') {
	    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	    exit(1);
	} else break;
    }
    if (argc<=1 || strcmp(argv[1], "-") == 0)
	ifd = stdin;
    else if((ifd = fopen(argv[1],"r")) == 0) {
	perror(argv[1]); exit(1);
    }
    outrun('!', 0);
    while((ch = fgetc(ifd)) != EOF) {
	/* These chars are RLE */
	m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	/* These ones are not */
	if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',') continue;
	/* Check for loop comments; ie: ][ comment ] */
	if (lc || (ch=='[' && lastch==']')) { lc += (ch=='[') - (ch==']'); continue; }
	if (lc) continue;
	/* Do the RLE */
	if (m && ch == lastch) { c++; continue; }
	/* Post the RLE token onward */
	if (c) outrun(lastch, c);
	c = m;
	lastch = ch;
	if (!m) {
	    /* Non RLE tokens here */
	    if (!b && ch == ']') continue; /* Ignore too many ']' */
	    b += (ch=='[') - (ch==']');
	    outrun(ch, 1);
	}
    }
    if (ifd != stdin) fclose(ifd);
    if(c) outrun(lastch, c);
    while(b>0){ outrun(']', 1); b--;} /* Not enough ']', add some. */
    outrun('~', 0);
    return 0;
}
