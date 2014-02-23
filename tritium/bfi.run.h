
extern int verbose;
extern int iostyle;
extern int do_run;

void * map_hugeram(void);
void unmap_hugeram(void);
void delete_tree(void);
void * tcalloc(size_t nmemb, size_t size);

int getch(int oldch);
void putch(int oldch);

