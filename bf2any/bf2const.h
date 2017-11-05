void outopt(int ch, int count);

struct mem { struct mem *p, *n; int is_set; int v; int cleaned, cleaned_val;};
extern struct mem *tape;

