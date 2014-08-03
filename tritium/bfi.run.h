
extern int verbose;
extern int iostyle;
extern int do_run;

void run_tree(void);
void * map_hugeram(void);
void unmap_hugeram(void);
void delete_tree(void);
void * tcalloc(size_t nmemb, size_t size);

void start_runclock(void);
void finish_runclock(void);

int getch(int oldch);
void putch(int oldch);

