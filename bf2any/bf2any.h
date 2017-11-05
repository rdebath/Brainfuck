
extern int tapelen;
extern int enable_optim;
extern int disable_init_optim;
extern int enable_debug;
extern const char * current_file;
extern char * extra_commands;

void outcmd(int ch, int count);
char * get_string(void);

/* Commons BE may override init value */
typedef int (check_arg_t)(const char * arg);
struct be_interface_s { check_arg_t *check_arg; } be_interface;
int disable_be_optim;
int cells_are_ints;
int bytecell;
int nobytecell;

/* This can be changed by the Makefile but everything must be recompiled. */
#ifndef BOFF
#define BOFF 256
#endif

#define tapeinit (enable_optim?BOFF:0)
#define tapesz   (tapelen+tapeinit)

/* Make a strdup() for old systems with an ok GCC */

#if defined(__GNUC__) \
    && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define UNUSED __attribute__ ((__unused__))

#if (_XOPEN_VERSION+0) < 500 && _POSIX_VERSION < 200809L
#if !defined(strdup) && !defined(_WIN32)
#define strdup(str) \
    ({char*_s=(str);int _l=strlen(_s)+1;void*_t=malloc(_l);if(_t)memcpy(_t,_s,_l);_t;})
#endif
#endif

#else
#define UNUSED
#define __attribute__(__ignored__)
#endif
