
extern int verbose;
extern int do_run;

extern double run_time, io_time;

extern int huge_ram_available;

void run_tree(void);
void * map_hugeram(void);
void unmap_hugeram(void);
void delete_tree(void);
void * tcalloc(size_t nmemb, size_t size);

int getch(int oldch);
int getint(int oldch);
void putch(int ch);
void putint(unsigned int ch);
