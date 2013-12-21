#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/wait.h>

/*
 * GCC-O0 translation from BF, runs at about 2,700,000,000 instructions per second.
 * GCC-O2 translation from BF, runs at about 4,300,000,000 instructions per second.
 */

extern int bytecell;

int ind = 0;
int runmode = 1;
FILE * ofd;
#define pr(s)           fprintf(ofd, "%*s" s "\n", ind*4, "")
#define prv(s,v)        fprintf(ofd, "%*s" s "\n", ind*4, "", (v))
#define prv2(s,v,v2)    fprintf(ofd, "%*s" s "\n", ind*4, "", (v), (v2))

static void compile_and_run(void);

int imov = 0;
char * opt_flag = "-O0";

static char tmpdir[] = "/tmp/bfrun.XXXXXX";
static char ccode_name[sizeof(tmpdir)+16];
static char dl_name[sizeof(tmpdir)+16];

int
check_arg(char * arg)
{
    if (strcmp(arg, "-O") == 0) return 1;
    if (strncmp(arg, "-O", 2) == 0) {
	opt_flag = arg;
	return 1;
    } else if (strcmp(arg, "-r") == 0) {
	runmode=1; return 1;
    }
    else if (strcmp(arg, "-d") == 0) {
	runmode=0; return 1;
    }
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
	case 'B': prv("v=*(m+=%d);", mov); return;
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
	if (runmode) {
	    if( mkdtemp(tmpdir) == 0 ) {
		perror("mkdtemp()");
		exit(1);
	    }
	    strcpy(ccode_name, tmpdir); strcat(ccode_name, "/bfpgm.c");
	    strcpy(dl_name, tmpdir); strcat(dl_name, "/bfpgm.so");
	    ofd = fopen(ccode_name, "w");
	} else
	    ofd = stdout;

	pr("#include <stdio.h>");
	pr("int brainfuck(void){");
	ind++;
	if (bytecell) {
	    pr("static char mem[30000];");
	    prv("register char *m = mem + %d;", BOFF);
	} else {
	    pr("static int mem[30000];");
	    prv("register int *m = mem + %d;", BOFF);
	}
	pr("register int v;");
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
    case ',': pr("v = getchar(); if(v!=EOF) *m = v;"); break;

    case '[': pr("while(*m) {"); ind++; break;
    case ']': ind--; pr("}"); break;
    }

    if (ch != '~') return;
    pr("return 0;\n}");

    if (!runmode) return;
    fclose(ofd);

    compile_and_run();
}

/*  Needs:   gcc -shared -fPIC -o libfoo.so foo.c
    And:     -ldl

    NB: -fno-asynchronous-unwind-tables
*/

static int loaddll(const char *);
static void (*runfunc)(void);
static void *handle;

static void
compile_and_run(void)
{
    char cmdbuf[256];
    int ret;

    sprintf(cmdbuf, "cc %s -shared -fPIC -o %s %s",
	    opt_flag, dl_name, ccode_name);
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

