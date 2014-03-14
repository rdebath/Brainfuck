#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef NO_DLOPEN
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#ifndef NO_LIBTCC
#include <libtcc.h>
#else
#warning "Compiling without libtcc support"
#endif

#include "bf2any.h"

/*
 * GCC-O0 translation from BF, runs at about 2,700,000,000 instructions per second.
 * GCC-O2 translation from BF, runs at about 4,300,000,000 instructions per second.
 * TCC translation from BF, runs at about 1,200,000,000 instructions per second.
 *
 * BCC translation from BF, runs at about 3,000,000,000 instructions per second.
 *	bcc -ansi -Ml -z -o bf bf.c
 */

int ind = 0;
enum { no_run, run_libtcc, run_dll } runmode = run_libtcc;
FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    fprintf(ofd, "%*s" s "\n", ind*4, "", (v), (v2))

#ifndef NO_LIBTCC
static void compile_and_run_libtcc(void);
#endif
#ifndef NO_DLOPEN
static void compile_and_run_dll(void);
#endif
static void print_cstring(void);

static int imov = 0;
static int verbose = 0;

static const char * cc_cmd = 0;
static const char * opt_flag = "-O0";
static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];

char * ccode = 0;
size_t ccodesize = 0;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strcmp(arg, "-savestring") == 0) return 1;
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-d      Dump code"
	"\n\t"  "-ldl    Use dlopen to run compiled code."
	"\n\t"  "-ltcc   Use libtcc to run code."
	"\n\t"  "-Cclang Choose a different C compiler"
	"\n\t"  "-Ox     Pass -Ox to C compiler."
	);
	return 1;
    } else
    if (strncmp(arg, "-O", 2) == 0) {
	opt_flag = arg; return 1;
    } else
    if (strncmp(arg, "-C", 2) == 0 && arg[2]) {
	cc_cmd = arg + 2; return 1;
    } else
    if (strcmp(arg, "-v") == 0) {
	verbose++; return 1;
    } else
    if (strcmp(arg, "-d") == 0) {
	runmode = no_run; return 1;
    } else
#ifndef NO_DLOPEN
    if (strcmp(arg, "-ldl") == 0) {
	runmode = run_dll; return 1;
    } else
#endif
#ifndef NO_LIBTCC
    if (strcmp(arg, "-ltcc") == 0) {
	runmode = run_libtcc; return 1;
    } else
#endif
	return 0;
}

void
outcmd(int ch, int count)
{
    if (imov && ch != '>' && ch != '<') {
	int mov = imov;
	imov = 0;

	switch(ch) {
	case '=': prv2("*(m+=%d) = %d;", mov, count); return;
	case '+': prv2("*(m+=%d) +=%d;", mov, count); return;
	case '-': prv2("*(m+=%d) -=%d;", mov, count); return;
	case 'B': prv("v= *(m+=%d);", mov); return;
	case 'M': prv2("*(m+=%d) += v*%d;", mov, count); return;
	case 'N': prv2("*(m+=%d) -= v*%d;", mov, count); return;
	case 'S': prv("*(m+=%d) += v;", mov); return;
	}

	if (mov > 0)
	    prv("m += %d;", mov);
	else if (mov < 0)
	    prv("m -= %d;", -mov);
    }

    switch(ch) {
    case '!':
#ifndef NO_LIBTCC
	if (runmode == run_libtcc) {
	    ofd = open_memstream(&ccode, &ccodesize);
	} else
#endif
#ifndef NO_DLOPEN
	if (runmode != no_run) {
	    runmode = run_dll;
	    if( mkdtemp(tmpdir) == 0 ) {
		perror("mkdtemp()");
		exit(1);
	    }
	    strcpy(ccode_name, tmpdir); strcat(ccode_name, "/bfpgm.c");
	    strcpy(dl_name, tmpdir); strcat(dl_name, "/bfpgm.so");
	    ofd = fopen(ccode_name, "w");
	} else
#endif
	{
	    runmode = no_run;
	    ofd = stdout;
	}

	/* Annoyingly most C compilers don't like this line. */
	/* pr("#!/usr/bin/tcc -run"); */

	pr("#include <stdio.h>");
	if (runmode == run_dll) {
	    pr("int brainfuck(void){");
	    ind++;
	} else {
	    pr("int main(void){");
	    ind++;
	}
	if (bytecell) {
	    pr("static char mem[30000];");
	    prv("register char *m = mem + %d;", BOFF);
	    pr("register int v;");
	} else {
	    pr("static int mem[30000];");
	    prv("register int v, *m = mem + %d;", BOFF);
	}
	if (runmode == no_run)
	    pr("setbuf(stdout,0);");
	break;

    case 'X': pr("fprintf(stderr, \"Infinite Loop\\n\"); exit(1);"); break;

    case '=': prv("*m = %d;", count); break;
    case 'B': pr("v = *m;"); break;
    case 'M': prv("*m += v*%d;", count); break;
    case 'N': prv("*m -= v*%d;", count); break;
    case 'S': pr("*m += v;"); break;
    case 'Q': prv("if(v) *m = %d;", count); break;
    case 'm': prv("if(v) *m += v*%d;", count); break;
    case 'n': prv("if(v) *m -= v*%d;", count); break;
    case 's': pr("if(v) *m += v;"); break;

    case '+': prv("*m +=%d;", count); break;
    case '-': prv("*m -=%d;", count); break;
    case '>': imov += count; break;
    case '<': imov -= count; break;
    case '.': pr("putchar(*m);"); break;
    case '"': print_cstring(); break;
    case ',': pr("v = getchar(); if(v!=EOF) *m = v;"); break;

    case '[': pr("while(*m) {"); ind++; break;
    case ']': ind--; pr("}"); break;
    }

    if (ch != '~') return;
    pr("return 0;\n}");

    if (runmode == no_run) return;

    fclose(ofd);
    setbuf(stdout,0);

#ifndef NO_LIBTCC
    if (runmode == run_libtcc)
	compile_and_run_libtcc();
    else
#endif
#ifndef NO_DLOPEN
	compile_and_run_dll()
#endif
	;
}

static void
print_cstring(void)
{
    char * str = get_string();
    char buf[256];
    int gotnl = 0, gotperc = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("puts(\"%s\");", buf);
	    } else if (gotperc)
		prv("printf(\"%%s\",\"%s\");", buf);
	    else
		prv("printf(\"%s\");", buf);
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str == '%') gotperc = 1;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    buf[outlen++] = *str;
	} else if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}

/*  Needs:   gcc -shared -fPIC -o libfoo.so foo.c
    And:     -ldl

    NB: -fno-asynchronous-unwind-tables
*/

/* If we're 32 bit on a 64bit or vs.versa. we need an extra option */
#ifndef CC
#if defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=2))
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
#elif defined(__TINYC__)
#define CC "tcc"
#else
#define CC "cc"
#endif
#endif

static int loaddll(const char *);
static void (*runfunc)(void);
static void *handle;

static void
compile_and_run_dll(void)
{
    char cmdbuf[256];
    int ret;
    const char * cc = CC;
    setbuf(stdout,0);

    if (cc_cmd) cc = cc_cmd;

    sprintf(cmdbuf, "%s %s -w -shared -fPIC -o %s %s",
	    cc, opt_flag, dl_name, ccode_name);
    if (verbose)
	fprintf(stderr, "Compiling with '%s'\n", cmdbuf);
    ret = system(cmdbuf);

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

    if (verbose)
	fprintf(stderr, "Loading with dlopen\n");
    loaddll(dl_name);

    unlink(ccode_name);
    unlink(dl_name);
    rmdir(tmpdir);

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
       POSIX specification of dlsym(). */

    *(void **) (&runfunc) = dlsym(handle, "brainfuck");

    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", error);
        exit(EXIT_FAILURE);
    }
    return 0;
}

#ifndef NO_LIBTCC
static void
compile_and_run_libtcc(void)
{
    TCCState *s;
    int rv;

    if (verbose)
	fprintf(stderr, "Compiling and running with libtcc\n");
    s = tcc_new();
    if (s == NULL) { perror("tcc_new()"); exit(7); }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_compile_string(s, ccode);

    rv = tcc_run(s, 0, 0);
    if (rv) fprintf(stderr, "tcc_run returned %d\n", rv);
    tcc_delete(s);
    free(ccode);
}
#endif
