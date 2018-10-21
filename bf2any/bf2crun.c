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
#include "move_opt.h"

/*
 * GCC-O0 translation from BF, runs at about 2,700,000,000 instructions per second.
 * GCC-O2 translation from BF, runs at about 4,300,000,000 instructions per second.
 * TCC translation from BF, runs at about 1,200,000,000 instructions per second.
 *
 * BCC translation from BF, runs at about 3,000,000,000 instructions per second.
 *	bcc -ansi -Ml -z -o bf bf.c
 */

static int ind = 0;
static int use_unistd = 0;
static enum { no_run, run_libtcc, run_dll } runmode = run_libtcc;
static FILE * ofd;
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
static void open_ofd(void);

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
static char * ccode = 0;
static size_t ccodesize = 0;
#endif

static check_arg_t fn_check_arg;
struct be_interface_s be_interface = { .check_arg = fn_check_arg,
    .cells_are_ints=1, .hasdebug=1};

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
    if (strcmp(arg, "-brain") == 0) {
	enable_debug = 1;
	extra_commands = "*/%{}!?:;$^#";
	return 1;
    } else
	return 0;
}

void
outcmd(int ch, int count)
{
    int mov = 0;

    if (ch == '#' || ch >= 256) {
	int ch2 = '~', count2 = 0, mov2 = 0;
	move_opt(&ch2, &count2, &mov2);
	if (count2 > 0) {
	    prv("m += %d;", count2);
	} else if (count2 < 0) {
	    prv("m -= %d;", -count2);
	}
    } else {
	move_opt(&ch, &count, &mov);
	if (ch == 0) return;
    }

    switch(ch) {
    case '!':
	open_ofd();

	/* Annoyingly most C compilers don't like this line. */
	/* pr("#!/usr/bin/tcc -run"); */

	if (use_unistd) {
	    if (enable_debug)
		pr("#include <stdio.h>");
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
	    pr("#define GETC(x) { int c=getchar(); if(c!=EOF) (x)=c; }");
	    pr("#define PUTC(x) putchar(x)");
	}

	pr("#ifndef TAPELEN");
	prv("#define TAPELEN %d", tapelen);
	pr("#endif");
	prv("#define TAPEOFF %d", tapeinit);

	if (enable_debug) {
	    pr("#ifndef do_dump");
	    pr("#define do_dump() call_dump(mem,m)");
	    if (bytecell)
		pr("void call_dump(char *mem, char *m)");
	    else
		pr("void call_dump(int *mem, int *m)");
	    pr("{");
	    ind++;
	    pr("int i;");
	    pr("fprintf(stderr,\"P[%%d]=%%d: \",m-(mem+TAPEOFF),*m);");
	    pr("for(i=0; i<16; i++) {");
	    ind++;
	    pr("fprintf(stderr,\" %%d\", mem[i+TAPEOFF]);");
	    pr("if (m==mem+i+TAPEOFF) fprintf(stderr,\"<\");");
	    ind--;
	    pr("}");
	    pr("fprintf(stderr,\"\\n\");");
	    ind--;
	    pr("}");
	    pr("#endif");
	}
	if (runmode == run_dll) {
	    pr("int brainfuck(void){");
	    ind++;
	} else {
	    pr("int main(void){");
	    ind++;
	}
	if (bytecell) {
	    pr("static unsigned char mem[TAPELEN+TAPEOFF];");
	    pr("register unsigned char *m = mem + TAPEOFF;");
	    pr("register int v;");
	} else {
	    pr("static unsigned int mem[TAPELEN+TAPEOFF];");
	    pr("register unsigned int v, *m = mem + TAPEOFF;");
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

    case '=': prv2("m[%d] = %d;", mov, count); return;
    case 'B': prv("v= m[%d];", mov); return;
    case 'M': prv2("m[%d] += v*%d;", mov, count); return;
    case 'N': prv2("m[%d] -= v*%d;", mov, count); return;
    case 'S': prv("m[%d] += v;", mov); return;
    case 'T': prv("m[%d] -= v;", mov); return;
    case '*': prv("m[%d] *= v;", mov); return;

    case 'C': prv2("m[%d] = v*%d;", mov, count); return;
    case 'D': prv2("m[%d] = -v*%d;", mov, count); return;
    case 'V': prv("m[%d] = v;", mov); return;
    case 'W': prv("m[%d] = -v;", mov); return;

    case '+': prv2("m[%d] += %d;", mov, count); return;
    case '-': prv2("m[%d] -= %d;", mov, count); return;
    case '>': prv("m += %d;", count); break;
    case '<': prv("m -= %d;", count); break;
    case '.': prv("PUTC(m[%d]);", mov); return;
    case '"': print_cstring(); break;
    case ',': prv("GETC(m[%d]);", mov); break;
    case '#': pr("do_dump();"); break;

    case '[': prv("while(m[%d]) {", mov); ind++; break;
    case ']':
	if (count > 0)
            prv("m += %d;", count);
        else if (count < 0)
            prv("m -= %d;", -count);
	ind--;
	pr("}");
	break;

    case 'I': prv("if(m[%d]) {", mov); ind++; break;
    case 'E':
	if (count > 0)
            prv("m += %d;", count);
        else if (count < 0)
            prv("m -= %d;", -count);
	ind--;
	pr("}");
	break;

    default:
	if (ch>=256)
	{
	    switch(extra_commands[ch-256])
	    {
	    case '*':
		pr("*m *= m[-1];");
		break;
	    case '/':
		pr("*m /= m[-1];");
		break;
	    case '%':
		pr("*m %%= m[-1];");
		break;
	    case '{':
		pr("{int f=*m; while(f-->0) {");
		ind++;
		break;
	    case '}':
		ind--;
		pr("}}");
		break;
	    case '!':
		pr("break;");
		break;
	    case '?':
		pr("if(*m) {");
		ind++;
		break;
	    case ':':
		ind--;
		pr("} else {");
		ind++;
		break;
	    case ';':
		ind--;
		pr("}");
		break;
	    case '$':
		pr("printf(\"%%d.%%02d\",*m/100,*m%%100);");
		break;
	    case '^':
		prv("m = mem + %d + *m;", tapeinit);
		break;
	    case '#':
		pr("do_dump();");
		break;
	    }
	}
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

static void
open_ofd(void)
{
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
}

#ifndef DISABLE_DLOPEN
/*  Needs:   gcc -shared -fpic -o libfoo.so foo.c
    And:     -ldl

    NB: -fno-asynchronous-unwind-tables
*/

/* If we're 32 bit on a 64bit or vs.versa. we need an extra option */
#ifndef CC
#if defined(__clang__) && (__clang_major__>=3) && defined(__i386__)
#define CC "clang -m32 -fwrapv"
#elif defined(__clang__) && (__clang_major__>=3) && defined(__amd64__)
#define CC "clang -m64 -fwrapv"
#elif defined(__PCC__)
#define CC "pcc"
#elif defined(__GNUC__) && ((__GNUC__>4) || (__GNUC__==4 && __GNUC_MINOR__>=1))
#if defined(__x86_64__)
#if defined(__ILP32__)
#define CC "gcc -mx32 -fwrapv"
#else
#define CC "gcc -m64 -fwrapv"
#endif
#elif defined(__i386__)
#define CC "gcc -m32 -fwrapv"
#else
#define CC "gcc -fwrapv"
#endif
#elif defined(__GNUC__) && (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ > 3)
#define CC "gcc -fwrapv"
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
    if (s == NULL) { perror("tcc_new() failed"); exit(7); }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_compile_string(s, ccode);
    free(ccode); ccode = 0; ccodesize = 0;

    rv = tcc_run(s, 0, 0);
    if (rv) { fprintf(stderr, "tcc_run() failed = %d\n", rv); exit(8); }
    tcc_delete(s);
}
#endif
