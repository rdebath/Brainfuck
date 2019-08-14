
extern int tapelen;
extern int tapeinit;
extern int enable_optim;
extern int disable_init_optim;
extern int enable_debug;
extern int bytecell;
extern const char * current_file;
extern char * extra_commands;

void outcmd(int ch, int count);
void add_string(int ch);
void flush_string(void);

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
