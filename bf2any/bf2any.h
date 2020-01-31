
/* Commons BE may override init value */
typedef int (check_arg_t)(const char * arg);
typedef void (gen_code_t)(int ch, int count, char * strn);
struct be_interface_s {
    check_arg_t *check_arg;
    gen_code_t *gen_code;
    int cells_are_ints;
    int bytesonly;
    int disable_be_optim;
    int hasdebug;
    int enable_chrtok;

    /* set by front end */
    int tape_init;
    int tape_len;
    int fe_enable_optim;
    int fe_bytecell;
    int disable_fe_optim;	/* Only for bf2bf */
    int fe_enable_debug;        /* Only for C generator */
    const char * fe_extra_commands;   /* Only for C generator */
} be_interface;

#define tapeinit (be_interface.tape_init)
#define tapesz   (be_interface.tape_len)
#define enable_optim	(be_interface.fe_enable_optim)
#define bytecell	(be_interface.fe_bytecell)
#define enable_debug	(be_interface.fe_enable_debug)
#define extra_commands	(be_interface.fe_extra_commands)

/* Make a strdup() for old systems with an ok GCC */

#if defined(__GNUC__) \
    && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define UNUSED __attribute__ ((__unused__))

#ifdef _POSIX_VERSION
#if (_XOPEN_VERSION+0) < 500 && _POSIX_VERSION < 200809L
#if !defined(strdup)
#define strdup(str) \
    ({char*_s=(str);int _l=strlen(_s)+1;void*_t=malloc(_l);if(_t)memcpy(_t,_s,_l);_t;})
#endif
#endif
#endif

#else
#define UNUSED
#define __attribute__(__ignored__)
#endif
