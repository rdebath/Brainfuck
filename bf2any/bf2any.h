
/* Commons BE may override init value */
typedef int (check_arg_t)(const char * arg);
typedef void (gen_code_t)(int ch, int count, char * strn);
struct be_interface_s {
    check_arg_t *check_arg;
    gen_code_t *gen_code;
    int cells_are_ints;
    int bytesonly;
    int disable_be_optim;
    int disable_fe_optim;
    int hasdebug;
    int enable_chrtok;
} be_interface;

extern struct fe_interface_s {
    int tape_init;
    int tape_len;
    int fe_enable_optim;
    int fe_bytecell;
    int fe_enable_debug;        /* Only for C generator */
    char * fe_extra_commands;   /* Only for C generator */
} fe_interface;

#define tapeinit (fe_interface.tape_init)
#define tapelen  (fe_interface.tape_len)
#define tapesz   (tapelen+tapeinit)
#define enable_optim	(fe_interface.fe_enable_optim)
#define bytecell	(fe_interface.fe_bytecell)
#define enable_debug	(fe_interface.fe_enable_debug)
#define extra_commands	(fe_interface.fe_extra_commands)

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
