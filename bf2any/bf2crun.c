#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef DISABLE_DLOPEN
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#ifndef DISABLE_LIBTCC
#include <libtcc.h>
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
int use_unistd = 0;
int use_mmove = 0;
enum { no_run, run_libtcc, run_dll } runmode = run_libtcc;
FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    fprintf(ofd, "%*s" s "\n", ind*4, "", (v), (v2))

#ifndef DISABLE_LIBTCC
static void compile_and_run_libtcc(void);
#endif
#ifndef DISABLE_DLOPEN
static void compile_and_run(void);
#endif
static void print_cstring(void);

static int imov = 0;
static int verbose = 0;

#ifndef DISABLE_DLOPEN
#define BFBASE "bfpgm"
static const char * cc_cmd = 0;
static const char * copt = "-O0";
static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];
static char obj_name[sizeof(tmpdir)+16];
static int in_one = 0;
static char pic_opt[8] = " -fpic";
#endif

#ifndef DISABLE_LIBTCC
char * ccode = 0;
size_t ccodesize = 0;
#endif

int cells_are_ints = 1;
static check_arg_t fn_check_arg;
struct be_interface_s be_interface = {fn_check_arg};

static int
fn_check_arg(const char * arg)
{
    if (strcmp(arg, "-#") == 0) return 1;
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-d      Dump code"
	"\n\t"  "-mmove  Use move merging translation."
	"\n\t"  "-unix   Use \"unistd.h\" for read/write."
#ifndef DISABLE_LIBTCC
	"\n\t"  "-ltcc   Use libtcc to run code."
#endif
#ifndef DISABLE_DLOPEN
	"\n\t"  "-ldl    Use dlopen to run compiled code."
	"\n\t"  "-Cclang Choose a different C compiler"
	"\n\t"  "-Ox     Pass -Ox to C compiler."
#endif
	);
	return 1;
    } else
#ifndef DISABLE_DLOPEN
    if (strncmp(arg, "-O", 2) == 0) {
	runmode = run_dll;  /* Libtcc doesn't optimise */
	copt = arg; return 1;
    } else
    if (strncmp(arg, "-C", 2) == 0 && arg[2]) {
	runmode = run_dll;  /* Running the C compiler, not libtcc */
	cc_cmd = arg + 2; return 1;
    } else
#endif
    if (strcmp(arg, "-v") == 0) {
	verbose++; return 1;
    } else
    if (strcmp(arg, "-d") == 0) {
	runmode = no_run; return 1;
    } else
    if (strcmp(arg, "-mmove") == 0) {
	use_mmove = 1; return 1;
    } else
    if (strcmp(arg, "-unix") == 0) {
	use_unistd = 1; return 1;
    } else
#ifndef DISABLE_DLOPEN
    if (strcmp(arg, "-ldl") == 0) {
	runmode = run_dll; return 1;
    } else
#endif
#ifndef DISABLE_LIBTCC
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

	if (use_mmove) {
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
	} else {
	    switch(ch) {
	    case '=': prv2("m[%d] = %d;", mov, count); return;
	    case 'B': prv("v= m[%d];", mov); return;
	    case 'M': prv2("m[%d] += v*%d;", mov, count); return;
	    case 'N': prv2("m[%d] -= v*%d;", mov, count); return;
	    case 'S': prv("m[%d] += v;", mov); return;
	    case 'Q': prv2("if(v) m[%d] = %d;", mov, count); return;
	    case 'm': prv2("if(v) m[%d] += v*%d;", mov, count); return;
	    case 'n': prv2("if(v) m[%d] -= v*%d;", mov, count); return;
	    case 's': prv("if(v) m[%d] += v;", mov); return;
	    case '+': prv2("m[%d] +=%d;", mov, count); return;
	    case '-': prv2("m[%d] -=%d;", mov, count); return;
	    case '.': prv("PUTC(m[%d]);", mov); return;
	    }
	}

	imov = 0;
	if (mov > 0)
	    prv("m += %d;", mov);
	else if (mov < 0)
	    prv("m -= %d;", -mov);
    }

    switch(ch) {
    case '!':
#ifndef DISABLE_LIBTCC
	if (runmode == run_libtcc) {
	    ofd = open_memstream(&ccode, &ccodesize);
	} else
#endif
#ifndef DISABLE_DLOPEN
	if (runmode != no_run) {
	    runmode = run_dll;
#if _POSIX_VERSION >= 200809L
	    if( mkdtemp(tmpdir) == 0 ) {
		perror("mkdtemp()");
		exit(1);
	    }
#else
#warning mkdtemp not used _POSIX_VERSION is too low, using mktemp instead
	    if (mkdir(mktemp(tmpdir), 0700) < 0) {
		perror("mkdir(mktemp()");
		exit(1);
	    }
#endif
	    strcpy(ccode_name, tmpdir); strcat(ccode_name, "/"BFBASE".c");
	    strcpy(dl_name, tmpdir); strcat(dl_name, "/"BFBASE".so");
	    strcpy(obj_name, tmpdir); strcat(obj_name, "/"BFBASE".o");
	    ofd = fopen(ccode_name, "w");
	} else
#endif
	{
	    runmode = no_run;
	    ofd = stdout;
	}

	/* Annoyingly most C compilers don't like this line. */
	/* pr("#!/usr/bin/tcc -run"); */

	if (use_unistd) {
	    pr("#include <unistd.h>");
	    if (bytecell) {
		pr( "#define GETC(x) read(0, &(x), 1)");
	    } else {
		pr( "#define GETC(x) "
		    "{ unsigned char b[1]; if(read(0, b, 1)>0) (x)=*b; }");
	    }
	    if (bytecell)
		pr( "#define PUTC(x) write(1, &(x), 1);");
	    else {
		pr( "#define PUTC(x) "
		    "{ char b[1]; b[0] = (x); write(1, b, 1); }");
	    }
	    pr( "#define PUTS(x) "
		"{ const char s[] = x; write(1,s,sizeof(s)-1); }");
	} else {
	    pr("#include <stdio.h>");
	    pr("#define GETC(x) { v=getchar(); if(v!=EOF) (x)=v; }");
	    pr("#define PUTC(x) putchar(x)");
	}
	if (runmode == run_dll) {
	    pr("int brainfuck(void){");
	    ind++;
	} else {
	    pr("int main(void){");
	    ind++;
	}
	if (bytecell) {
	    prv("static char mem[%d];", tapesz);
	    prv("register char *m = mem + %d;", tapeinit);
	    pr("register int v;");
	} else {
	    prv("static int mem[%d];", tapesz);
	    prv("register int v, *m = mem + %d;", tapeinit);
	}
	if (runmode == no_run && !use_unistd)
	    pr("setbuf(stdout,0);");
	break;

    case 'X':
	if (use_unistd)
	    pr("{char e[]=\"Infinite Loop\\n\"; write(2,e,sizeof(e)-1); return 1;}");
	else
	    pr("fprintf(stderr, \"Infinite Loop\\n\"); return 1;");
	break;

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
    case '.': pr("PUTC(*m);"); break;
    case '"': print_cstring(); break;
    case ',': pr("GETC(*m);"); break;
    case '#': if (runmode == no_run) pr("do_dump();"); break;

    case '[': pr("while(*m) {"); ind++; break;
    case ']': ind--; pr("}"); break;
    }

    if (ch != '~') return;
    pr("return 0;\n}");

    if (runmode == no_run) return;

    fclose(ofd);
    setbuf(stdout,0);

#ifndef DISABLE_LIBTCC
    if (runmode == run_libtcc)
	compile_and_run_libtcc();
    else
#endif
#ifndef DISABLE_DLOPEN
	compile_and_run()
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
	    if (use_unistd) {
		prv("PUTS(\"%s\")", buf);
	    } else if (gotnl) {
		buf[outlen-2] = 0;
		prv("puts(\"%s\");", buf);
	    } else if (gotperc)
		prv("printf(\"%%s\", \"%s\");", buf);
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
	} else if (*str == '\r') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'r';
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

#ifndef DISABLE_DLOPEN
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

    loaddll(dl_name);

    unlink(ccode_name);
    unlink(dl_name);
    unlink(obj_name);
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
#endif

#ifndef DISABLE_LIBTCC
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
