
extern int bytecell;
extern int tapelen;
extern int enable_optim;
extern int enable_be_optim;
extern int enable_bf_optim;
extern int enable_mov_optim;
extern int disable_init_optim;
extern int enable_debug;
extern const char * current_file;

void outcmd(int ch, int count);
void outopt(int ch, int count);
int check_arg(const char * arg);

char * get_string(void);

/* Add default so that code is valid without special compile options */
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
