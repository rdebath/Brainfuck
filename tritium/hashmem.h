
/*
 * To use create a variable 'htab' for the first argument.
 *
 * struct hashtab * htab[1] = {0};
 *
 * The string pointed to be 'name' argument is copied into the table so
 * it can be a temp buffer. But only the pointer to the 'value' entity
 * is stored in the table so the entity must be permanent.
 *
 * The recommended way to delete the table is:
 *
 * foreach_entry(htab, delete_htab_entry);
 * void delete_htab_entry(word, value) {
 *   set_entry(htab, word, 0);
 *   clear(value);
 *   free(value);
 * }
 *
 */

struct hashtab;
typedef void (*hashfunc_p)(char * word, void * value);
char * set_entry(struct hashtab ** hashtab, char * name, void * value);
void * find_entry(struct hashtab ** hashtab, char * name);
void foreach_entry(struct hashtab ** hashtab, hashfunc_p func);

void hash_stats(struct hashtab ** phashtab, int * size, int * words);

#ifdef TEST
/* Quick and dirty integer varients */
void * find_ientry(struct hashtab ** phashtab, int keyval);
char * set_ientry(struct hashtab ** phashtab, int keyval, void * value);
#endif
