void outopt(int ch, int count);
void outcmd(int ch, int count);
void add_string(int ch);
void flush_string(void);

struct mem { struct mem *p, *n; int is_set; int v; int cleaned, cleaned_val;};
extern struct mem *tape;

extern int disable_init_optim;
