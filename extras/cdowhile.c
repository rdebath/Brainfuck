/* This is a "jit" interpreter for a "Do While" variant of Brainfuck.
 * Unexpectedly, it appears to be Turing complete.
 *
 * THIS IS NOT AN INTERPRETER FOR THE brainfuck LANGUAGE!
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

int ind = 0;
int use_unistd = 0;
const char * current_file;
int enable_debug = 0;
int tapelen = 30000;

FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    fprintf(ofd, "%*s" s "\n", ind*4, "", (v), (v2))

static void compile_and_run(void);
int check_argv(const char * arg);
void outcmd(int ch, int count);

static int verbose = 0;

#define BFBASE "bfpgm"
static const char * cc_cmd = 0;
static const char * copt = "-O0";
static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];
static char obj_name[sizeof(tmpdir)+16];
static int in_one = 0;
static char pic_opt[8] = " -fpic";

/*
 */
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
	    fprintf(stderr, "%s\n",
	    "\t"    "-h      This message"
	    "\n\t"  "-M30000 Set length of tape."
	    "\n\t"  "-unix   Use \"unistd.h\" for read/write."
	    "\n\t"  "-Cclang Choose a different C compiler"
	    "\n\t"  "-Ox     Pass -Ox to C compiler."
	    );

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
    outcmd('!', 0);
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
	    /* These chars are RLE */
	    m = (ch == '>' || ch == '<' || ch == '+' || ch == '-');
	    /* These ones are not */
	    if(!m && ch != '[' && ch != ']' && ch != '.' && ch != ',' &&
		(ch != '#' || !enable_debug) ) continue;
	    /* Check for loop comments; ie: ][ comment ] */
	    if (lc || (ch=='[' && lastch==']' )) {
		lc += (ch=='[') - (ch==']'); continue;
	    }
	    if (lc) continue;
	    cf=1;
	    /* Do the RLE */
	    if (m && ch == lastch) { c+= 1; continue; }
	    /* Post the RLE token onward */
	    if (c) outcmd(lastch, c);
	    if (!m) {
		/* Non RLE tokens here */
		if (!b && ch == ']') continue; /* Ignore too many ']' */
		b += (ch=='[') - (ch==']');
		if (lastch == '[' && ch == ']') outcmd('X', 1);
		outcmd(ch, 1);
		c = 0;
	    } else
		c = 1;
	    lastch = ch;
	}
	if (ifd != stdin) fclose(ifd);
    }
    if(c) outcmd(lastch, c);
    while(b>0){ outcmd(']', 1); b--;} /* Not enough ']', add some. */
    if (enable_debug && lastch != '#') outcmd('#', 0);
    outcmd('~', 0);
    return 0;
}

int
check_argv(const char * arg)
{
    if (strcmp(arg, "-#") == 0) {
	enable_debug++;
    } else if (strcmp(arg, "-h") == 0) {
	return 0;
    } else if (strncmp(arg, "-M", 2) == 0 && arg[2] != 0) {
	tapelen = strtoul(arg+2, 0, 10);
	if (tapelen < 1) tapelen = 30000;
	return 1;
    } else
    if (strncmp(arg, "-O", 2) == 0) {
	copt = arg; return 1;
    } else
    if (strncmp(arg, "-C", 2) == 0 && arg[2]) {
	cc_cmd = arg + 2; return 1;
    } else
    if (strcmp(arg, "-v") == 0) {
	verbose++; return 1;
    } else
    if (strcmp(arg, "-unix") == 0) {
	use_unistd = 1; return 1;
    } else
	return 0;
    return 1;
}

void
outcmd(int ch, int count)
{
    switch(ch) {
    case '!':
	if( mkdtemp(tmpdir) == 0 ) {
	    perror("mkdtemp()");
	    exit(1);
	}
	strcpy(ccode_name, tmpdir); strcat(ccode_name, "/"BFBASE".c");
	strcpy(dl_name, tmpdir); strcat(dl_name, "/"BFBASE".so");
	strcpy(obj_name, tmpdir); strcat(obj_name, "/"BFBASE".o");
	ofd = fopen(ccode_name, "w");

	if (use_unistd) {
	    pr("#include <unistd.h>");
	    pr( "#define GETC(x) read(0, &(x), 1)");
	    pr( "#define PUTC(x) { char v=(x); if(v) write(1, &v, 1);}");
	} else {
	    pr("#include <stdio.h>");
	    pr("#define GETC(x) { int v=getchar(); if(v!=EOF) (x)=v; }");
	    pr("#define PUTC(x) { char v=(x); if(v) putchar(v); }");
	}
	pr("int brainfuck(void){");
	ind++;
	prv("static char mem[%d];", tapelen);
	pr("register char *m = mem;");
	break;

    case 'X':
	if (use_unistd)
	    pr("{char e[]=\"Infinite Loop\\n\"; write(2,e,sizeof(e)-1); return 1;}");
	else
	    pr("fprintf(stderr, \"Infinite Loop\\n\"); return 1;");
	break;

    case '+': prv("*m +=%d;", count); break;
    case '-': prv("*m -=%d;", count); break;
    case '>': prv("m += %d;", count); break;
    case '<': prv("m -= %d;", count); break;
    case '.': pr("PUTC(*m);"); break;
    case ',': pr("GETC(*m);"); break;
    case '[': pr("do {"); ind++; break;
    case ']': ind--; pr("} while(*m);"); break;
    }

    if (ch != '~') return;
    pr("return 0;\n}");

    fclose(ofd);
    setbuf(stdout,0);

    compile_and_run();
}

/*  Needs:   gcc -shared -fpic -o libfoo.so foo.c
    And:     -ldl

    NB: -fno-asynchronous-unwind-tables
*/

/* If we're 32 bit on a 64bit or vs.versa. we need an extra option */
#ifndef CC
#if defined(__clang__) && (__clang_major__>=3) && defined(__i386__)
#define CC "clang -m32"
#elif defined(__clang__) && (__clang_major__>=3) && defined(__amd64__)
#define CC "clang -m64"
#elif defined(__PCC__)
#define CC "pcc"
#elif defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=4))
#if defined(__x86_64__)
#if defined(__ILP32__)
#define CC "gcc -mx32"
#else
#define CC "gcc -m64"
#endif
#elif defined(__i386__)
#define CC "gcc -m32"
#else
#define CC "gcc"
#endif
#elif defined(__GNUC__)
#define CC "gcc"
#elif defined(__TINYC__)
#define CC "tcc"
#else
#define CC "cc"
#endif
#endif

typedef void (*voidfnp)(void);
static int loaddll(const char *);
static voidfnp runfunc;
static void *handle;

static void
compile_and_run(void)
{
    char cmdbuf[256];
    int ret;
    const char * cc = CC;

    if (cc_cmd) cc = cc_cmd;

    if (in_one) {
	if (verbose)
	    fprintf(stderr,
		"Running C Code using \"%s%s %s -shared\" and dlopen().\n",
		cc,pic_opt,copt);

	sprintf(cmdbuf, "%s%s %s -shared -o %s %s",
		    cc, pic_opt, copt, dl_name, ccode_name);
	ret = system(cmdbuf);
    } else {
	if (verbose)
	    fprintf(stderr,
		"Running C Code using \"%s%s %s\", link -shared and dlopen().\n",
		cc,pic_opt,copt);

	/* Like this so that ccache has a good path and distinct compile. */
	sprintf(cmdbuf, "cd %s; %s%s %s -c -o %s %s",
		tmpdir, cc, pic_opt, copt, BFBASE".o", BFBASE".c");
	ret = system(cmdbuf);

	if (ret != -1) {
	    sprintf(cmdbuf, "cd %s; %s%s -shared -o %s %s",
		    tmpdir, cc, pic_opt, dl_name, BFBASE".o");
	    ret = system(cmdbuf);
	}
    }

    if (ret == -1) {
	perror("Calling C compiler failed");
	exit(1);
    }
    if (WIFEXITED(ret)) {
	if (WEXITSTATUS(ret)) {
	    fprintf(stderr, "Compile failed.\n");
	    exit(WEXITSTATUS(ret));
	}
    } else {
	if (WIFSIGNALED(ret)) {
	    if( WTERMSIG(ret) != SIGINT && WTERMSIG(ret) != SIGQUIT)
		fprintf(stderr, "Killed by SIGNAL %d.\n", WTERMSIG(ret));
	    exit(1);
	}
	perror("Abnormal exit");
	exit(1);
    }

    if (verbose) fprintf(stderr, "DLL compiled, loading\n");
    loaddll(dl_name);

    unlink(ccode_name);
    unlink(dl_name);
    unlink(obj_name);
    rmdir(tmpdir);

    if (verbose) fprintf(stderr, "Running compiled code.\n");
    (*runfunc)();

    dlclose(handle);
}

int
loaddll(const char * dlname)
{
    char *error;

    handle = dlopen(dlname, RTLD_LAZY);
    if (!handle) {
	fprintf(stderr, "%s\n", dlerror());
	exit(EXIT_FAILURE);
    }

    dlerror();    /* Clear any existing error */

    /* The C99 standard leaves casting from "void *" to a function
       pointer undefined.  The assignment used below is the POSIX.1-2003
       (Technical Corrigendum 1) workaround; see the Rationale for the
       POSIX specification of dlsym().
     *						-- Linux man page dlsym()
     */

    *(void **) (&runfunc) = dlsym(handle, "brainfuck");

    if ((error = dlerror()) != NULL)  {
	fprintf(stderr, "%s\n", error);
	exit(EXIT_FAILURE);
    }
    return 0;
}
