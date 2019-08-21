#include "bf2any.h"
#include "ov_int.h"

void outopt(int ch, int count);
void outcmd(int ch, int count);
void add_string(int ch);
void flush_string(void);

struct mem { struct mem *p, *n; int is_set; int v; int cleaned, cleaned_val;};
extern struct mem *tape;

extern int disable_init_optim;
extern int tape_is_all_blank;
void outtxn(int ch, int count);

