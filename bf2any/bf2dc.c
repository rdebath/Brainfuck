#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
 * Unix/GNU dc translation from BF, runs at about 1,500,000 instructions per second.
		  If malloc is avoided ...        2,600,000
 */

/*
 *  There are two other methods of storing the tape.
 *
 *  1)  Use a register to stack the left of the tape and the main stack
 *      for the right hand tape.
 *      This cannot use run length encoding.
 *
 *  2)  I could use a pair of variables and use divmod to move bytes from
 *      one to the other. This means we can't have bignum cells.
 *      Run length encoding is possible. (divmod by 256^N)
 *
 *  The bc(1) was originally a preprocessor for dc(1) and can do nothing
 *  more than dc(1) can do. In addition there is NO way at all of getting
 *  additional input into a bc(1) script (the "," command). I therefor
 *  have not done a bc(1) translation. Note, however, the bc(1) clone and
 *  considerable enhancement nickle(1) is still a candidate.
 *
 *  NB: "USE_STACK" is incomplete (Only works with recent DC and no EOF).
 */

extern int bytecell;

int do_input = 0;
int ind = 0;
#define I printf("%*s", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")

FILE * ofd;
int outputmode = 3;
#ifdef BFPROG
int pipemode = 1;
#include BFPROG
#else
int pipemode = 0;
#endif

int
check_arg(char * arg)
{
#ifndef USE_STACK
    if (strcmp(arg, "-O") == 0) return 1;
    else
#endif
    if (strcmp(arg, "-d") == 0) {
	pipemode=0; return 1;
    } else if (strcmp(arg, "-p") == 0) {
	pipemode=1; return 1;
    } else if (strcmp(arg, "-r") == 0) {
	pipemode=1; return 1;
    } else if (strcmp(arg, "-t") == 0) {
	outputmode=1; return 1;
    } else if (strcmp(arg, "-g") == 0) {
	outputmode=2; return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-d      Dump code (',' command is error)"
	"\n\t"  "-r      Run program (input will be requested before running)"
	"\n\t"  "-t      Traditional dc mode"
	"\n\t"  "-g      GNU dc mode (default is to autodetect)");
	return 1;
    } else
	return 0;
}

/* Note: calling exit() isn't posix */
void endprog(int s) { exit(0); }

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
#ifdef BFPROG
	if (ifd == stdin && pipemode) {
	    ifd = fmemopen(bfprog, bfprog_len, "r");
	}
#endif
	if (pipemode) ofd = popen("dc", "w"); else ofd = stdout;
	if (outputmode == 2)
	    pr("#!/usr/bin/dc");
	prv("[%dsp", BOFF);
	break;

#ifndef USE_STACK
    case '=': prv("%dlp:a", count); break;
    case 'B': pr("lp;asV"); break;
    case 'M': prv("lp;alV%d*+lp:a", count); break;
    case 'N': prv("lp;alV_%d*+lp:a", count); break;
    case 'S': pr("lp;alV+lp:a"); break;
    case 'Q': prv("[%dlp:a]svlV0!=v", count); break;
    case 'm': prv("[lp;alV%d*+lp:a]svlV0!=v", count); break;
    case 'n': prv("[lp;alV_%d*+lp:a]svlV0!=v", count); break;
    case 's': pr("[lp;alV+lp:a]svlV0!=v"); break;

    case '+': prv("lp;a%d+lp:a", count); break;
    case '-': prv("lp;a%d-lp:a", count); break;
    case '<': prv("lp%d-sp", count); break;
    case '>': prv("lp%d+sp", count); break;
    case '[': pr("["); break;
    case ']':
	if(bytecell)
	    pr("lmx0!=b]Sblmx0!=bLbc");
	else
	    pr("lp;a0!=b]Sblp;a0!=bLbc");
	break;
#endif

#ifdef USE_STACK
    case '>': while(count-->0) pr("SL z [0]sq 0=q"); break;
    case '<': while(count-->0) pr("LL"); break;
    case '+': prv("%d+", count); break;
    case '-': prv("%d-", count); break;
    case '[': pr("["); break;
    case ']':
	if (bytecell)
	    pr("lmx d0!=b]Sb lmx d0!=b 0sbLbd!=.");
	else
	    pr("d0!=b]Sbd0!=b 0sbLbd!=.");
	break;
#endif

    case ',': pr("lix"); do_input=1; break;
    case '.': pr("lox"); break;
    }

    if (ch != '~') return;

    pr("q]SF\n");

#ifndef USE_STACK
    if (pipemode) {
	pr("[?lp:a]si");
    } else {
	if (do_input) {
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
	}
    }

    if (outputmode&1) {
	int i;
	if (outputmode & 2)
	    fprintf(ofd, "[");
	for (i=1; i<127; i++) {
	    if (i == '\n') {
		fprintf(ofd, "[[\n]P]%d:C\n", i);
	    } else if ( i >= ' ' && i <= '~' &&
			i != '[' && i != ']' && i != '\\' && i != '|') {
		fprintf(ofd, "[[%c]P]%d:C", i, i);
	    } else
	    if (i < 100) {
		fprintf(ofd, "[%dP]%d:C", i, i);
	    }
	    if (i%8 == 7) fprintf(ofd, "\n");
	}
	if (outputmode & 2)
	    fprintf(ofd, "]sZ");
	fprintf(ofd, "\n");
    }

    switch(outputmode)
    {
    case 1:
	fprintf(ofd, "[256+]sM [lp;a 256 %% d0>M]sm\n");
	fprintf(ofd, "[lmx;Cx]so\n");
	break;

    case 2:
	fprintf(ofd, "[lp;aaP]so\n");
	break;

    default:
	/* Note: dc.sed works in traditional mode, but as it takes minutes
	   just to print "Hello World!" there's not much point doing an auto
	   detection. However, if wanted the best method seems to be to check
	   that the comment indicator '#' also works for new dc(1). */

	/* Use 'Z' command. Detects dc.sed as new style. */
	fprintf(ofd, "[256+]sM [lp;a 256 %% d0>M]sm\n");
	fprintf(ofd, "[1   [lmxaP]so ]sB\n");
	fprintf(ofd, "[lZx [lmx;Cx]so ]sA\n");
	fprintf(ofd, "0 0 [o] Z 1=B 0=A 0sA 0sB 0sZ c\n");
	break;
    }
#endif

#ifdef USE_STACK
    if (bytecell)
	fprintf(ofd, "[256+]sM [256 %% d0>M]sm\n");
    fprintf(ofd, "[daP]so\n");
    if (pipemode)
	fprintf(ofd, "[d!=.?]si\n");
    else
	fprintf(ofd, "[]si\n");
    fprintf(ofd, "0\n");
#endif

    fprintf(ofd, "LFx ");
    if (pipemode) {

	if (do_input) {
	    signal(SIGCHLD, endprog);
	    fflush(ofd);
	    while((ch = fgetc(stdin)) != EOF) {
		fprintf(ofd, "%d\n", ch);
		fflush(ofd);
	    }
	}
	pclose(ofd);
    }
    fprintf(ofd, "\n");
}


#ifdef FOR_DCSED
    {
	int i;
	fprintf(ofd, "[");
	for (i=1; i<127; i++) {
	    int okay = 1;
	    if (i == '\n') {
		fprintf(ofd, "[[\n]P]sC\n");
	    } else if ( i >= ' ' && i <= '~' &&
			i != '[' && i != ']' && i != '\\' && i != '|') {
		fprintf(ofd, "[[%c]P]sC", i);
	    } else
	    if (i < 100) {
		fprintf(ofd, "[%dP]sC", i);
	    }
	    else
		okay = 0;
	    if (okay)
		fprintf(ofd, "d%d=C", i);
	    fprintf(ofd, "\n");
	}
	fprintf(ofd, "]sZ");
	fprintf(ofd, "\n");
	fprintf(ofd, "[256+]sM [lp;a 256 %% d0>M]sm\n");
	fprintf(ofd, "[lmxlZxd!=.]so\n");
    }
#endif

