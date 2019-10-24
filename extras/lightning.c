/*
 * This is a very simple implementation of a BF interpreter using 
 * GNU lightning version 2.
 *
 * The only "optimisation" is run length encoding.
 * This provides a baseline to measure optimisations against.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TAPELEN
#define TAPELEN 30000
#endif

int bytecell = 1;
int tapelen = TAPELEN;
int enable_debug;
char * current_file;

void outrun(int ch, int count);

int
check_argv(const char * arg)
{
    if (strcmp(arg, "-i") == 0) {
	bytecell=0;

    } else if (strcmp(arg, "-h") == 0) {
	return 0;
    } else if (strncmp(arg, "-M", 2) == 0 && arg[2] != 0) {
	tapelen = strtoul(arg+2, 0, 10);
	if (tapelen < 1) tapelen = TAPELEN;
	return 1;
    }
    else return 0;
    return 1;
}

int
main(int argc, char ** argv)
{
    char * pgm = argv[0];
    int ch, lastch=']', c=0, m, b=0, lc=0, ar, cf=0;
    FILE * ifd;
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
	    "\n\t"  "-i      Use int cells"
	    "\n\t"  "-M<num> Set length of tape, default is ", TAPELEN,
	    "");

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
		lc || b || (c==0 && cf==0))) {
	    if (ch == ' ' || ch == '\t') continue;
	    /* These chars are RLE */
	    m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	    /* These ones are not */
	    if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',' &&
		(ch != '#' || !enable_debug)) continue;
	    /* Check for loop comments; ie: ][ comment ] */
	    if (lc || (ch=='[' && lastch==']')) {
		lc += (ch=='[') - (ch==']'); continue;
	    }
	    if (lc) continue;
	    cf=1;
	    /* Do the RLE */
	    if (m && ch == lastch) { c++; continue; }
	    /* Post the RLE token onward */
	    if (c) outrun(lastch, c);
	    if (!m) {
		/* Non RLE tokens here */
		if (!b && ch == ']') continue; /* Ignore too many ']' */
		b += (ch=='[') - (ch==']');
		if (lastch == '[' && ch == ']') outrun('X', 1);
		outrun(ch, 1);
		c = 0;
	    } else
		c = 1;
	    lastch = ch;
	}
	if (ifd != stdin) fclose(ifd);
    }
    if(c) outrun(lastch, c);
    while(b>0){ outrun(']', 1); b--;} /* Not enough ']', add some. */
    setbuf(stdout, 0);
    outrun('~', 0);
    return 0;
}

#include <lightning.h>

static jit_state_t *_jit;

/*
 * For i386 JIT_V0 == ebx, JIT_V1 == esi, JIT_V2 == edi
 *          JIT_R0 == eax, JIT_R1 == ecx, JIT_R2 == edx
 */

#define REG_P   JIT_V1
#define REG_ACC JIT_R0
#define REG_A1  JIT_R1

typedef void (*codeptr_t)(void*);
static codeptr_t codeptr;

jit_node_t    *start, *end;     /* For size of code */
jit_node_t** loopstack = 0;

int tape_step = 1;
int maxstack = 0, stackptr = 0;

void
outrun(int ch, int count)
{
    jit_node_t    *argp;

    switch(ch) {
    case '!':
	if (bytecell) tape_step = 1; else
	tape_step = sizeof(int);

        init_jit(NULL); // argv[0]);
	_jit = jit_new_state();
	start = jit_note(__FILE__, __LINE__);
	jit_prolog();

	/* Get the data area pointer */
	argp = jit_arg();
	jit_getarg(REG_P, argp);
	break;

    case '~':
	jit_ret();
	jit_epilog();
	end = jit_note(__FILE__, __LINE__);
	codeptr = jit_emit();
	jit_clear_state();
	codeptr(calloc(tape_step, tapelen));
	jit_destroy_state();
	finish_jit();
	codeptr = 0;
	if (loopstack) { free(loopstack); loopstack = 0; }
	break;

    case '>':
	jit_addi(REG_P, REG_P, count * tape_step);
	break;

    case '<':
	jit_subi(REG_P, REG_P, count * tape_step);
	break;

    case '+':
	if (tape_step>1)
	    jit_ldr_i(REG_ACC, REG_P);
	else
	    jit_ldr_uc(REG_ACC, REG_P);
	jit_addi(REG_ACC, REG_ACC, count);
	if (tape_step>1)
	    jit_str_i(REG_P, REG_ACC);
	else
	    jit_str_c(REG_P, REG_ACC);
	break;

    case '-':
	if (tape_step>1)
	    jit_ldr_i(REG_ACC, REG_P);
	else
	    jit_ldr_uc(REG_ACC, REG_P);
	jit_subi(REG_ACC, REG_ACC, count);
	if (tape_step>1)
	    jit_str_i(REG_P, REG_ACC);
	else
	    jit_str_c(REG_P, REG_ACC);
	break;

    case '[':
	if (tape_step>1)
	    jit_ldr_i(REG_ACC, REG_P);
	else
	    jit_ldr_uc(REG_ACC, REG_P);

	if (stackptr >= maxstack) {
	    loopstack = realloc(loopstack,
			((maxstack+=32)+2)*sizeof(*loopstack));
	    if (loopstack == 0) {
		perror("loop stack realloc failure");
		exit(1);
	    }
	}

	loopstack[stackptr] = jit_beqi(REG_ACC, 0);
	loopstack[stackptr+1] = jit_label();
	stackptr += 2;
	break;

    case ']':
	if (tape_step>1)
	    jit_ldr_i(REG_ACC, REG_P);
	else
	    jit_ldr_uc(REG_ACC, REG_P);

	stackptr -= 2;
	if (stackptr < 0) {
	    fprintf(stderr, "Code gen failure: Stack pointer negative.\n");
	    exit(1);
	}

	{
	    jit_node_t *ref;
	    ref = jit_bnei(REG_ACC, 0);
	    jit_patch_at(ref, loopstack[stackptr+1]);
	    jit_patch(loopstack[stackptr]);
	}
	break;

    case '.':
	if (tape_step>1)
	    jit_ldr_i(REG_ACC, REG_P);
	else
	    jit_ldr_uc(REG_ACC, REG_P);

	jit_prepare();
	jit_pushargr(REG_ACC);
	jit_finishi(putchar);
	break;

    case ',':
	jit_prepare();
	jit_pushargr(REG_ACC);
	jit_finishi(getchar);
	jit_retval(REG_ACC);

	{
	    jit_node_t *lbl = jit_beqi(REG_ACC, EOF);

	    if (tape_step>1)
		jit_str_i(REG_P, REG_ACC);
	    else
		jit_str_c(REG_P, REG_ACC);

	    jit_patch(lbl);
	}
	break;
    }
}
