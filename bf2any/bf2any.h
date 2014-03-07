
extern int bytecell;
extern int enable_optim;
extern int enable_debug;
extern int keep_dead_code;
extern const char * current_file;

void outcmd(int ch, int count);
void outopt(int ch, int count);
int check_arg(const char * arg);

char * get_string(void);

