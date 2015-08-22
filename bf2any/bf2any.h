
extern int bytecell;
extern int tapelen;
extern int enable_optim;
extern int enable_be_optim;
extern int enable_bf_optim;
extern int enable_debug;
extern int keep_dead_code;
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
