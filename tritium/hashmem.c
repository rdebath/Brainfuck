#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmem.h"

#undef TEST
struct hashtab {
   struct hashentry ** hashtable;
   int  hashsize; /*  = 0xFF; */ /* 2^X -1 */
   int total_words;
};

struct hashentry
{
   struct hashentry * next;
   void * value;
   char   word[1];
};

static int hashvalue(char * word, int hashsize);
static void rehash_table(struct hashtab* hashtab);

void
hash_stats(struct hashtab ** phashtab, int * size, int * words)
{
   struct hashtab * hashtab = *phashtab;
   if( hashtab == 0 || hashtab->hashsize == 0 ) {
      if(size) *size = 0;
      if(words) *words = 0;
   } else {
      if(size) *size = hashtab->hashsize;
      if(words) *words = hashtab->total_words;
   }
}

void *
find_entry(struct hashtab ** phashtab, char * word)
{
   int hash_val;
   struct hashentry * hashline;
   struct hashtab * hashtab = *phashtab;
   if( hashtab == 0 || hashtab->hashsize == 0 ) return 0;
   hash_val = hashvalue(word, hashtab->hashsize);

   hashline = hashtab->hashtable[hash_val];

   for(; hashline; hashline = hashline->next) {
      if(word[0] != hashline->word[0])     continue;
      if(strcmp(word, hashline->word) )    continue;
      return hashline->value;
   }
   return 0;
}

char *
set_entry(struct hashtab ** phashtab, char * word, void * value)
{
   int hash_val;
   struct hashentry * hashline, *prev;
   int chain_length = 0;
   struct hashtab * hashtab = *phashtab;

   if( hashtab == 0 )
      hashtab = *phashtab = calloc(sizeof(struct hashtab), 1);

   if( hashtab == 0 ) return 0;

   if (hashtab->hashsize == 0 ) {
      memset(hashtab, 0, sizeof(struct hashtab));
      hashtab->hashsize = 0xF; /* 2^X -1 */
   }

   hash_val = hashvalue(word, hashtab->hashsize);

   if( hashtab->hashtable ) {
      hashline = hashtab->hashtable[hash_val];

      for(prev=0; hashline; prev=hashline, hashline = hashline->next) {
	 chain_length++;
	 if(word[0] != hashline->word[0])     continue;
	 if(strcmp(word, hashline->word) )    continue;
	 if( value ) hashline->value = value;
	 else {
	    if( prev == 0 ) hashtab->hashtable[hash_val] = hashline->next;
	    else            prev->next = hashline->next;
	    free(hashline);
	    hashtab->total_words--;

	    /* Delete last one, structure goes too. */
	    if (hashtab->total_words <= 0) {
	       memset(hashtab->hashtable, 1, (hashtab->hashsize)*sizeof(char*));
	       free(hashtab->hashtable);
	       memset(hashtab, 1, sizeof(struct hashtab));
	       free(hashtab);
	       *phashtab = 0;
	    }
	    return 0;
	 }
	 return hashline->word;
      }
   }

   if( value == 0 ) return 0;
   if( hashtab->hashtable == 0 ) {
      int hashsize = hashtab->hashsize;
      struct hashentry ** hashtable;
      hashtable = calloc((hashsize+1), sizeof(char*));
      if( hashtable == 0 ) return 0;
      hashtab->hashtable = hashtable;
   }

   hashline = malloc(sizeof(struct hashentry)+strlen(word));
   if( hashline == 0 ) return 0;
   else {
      hashtab->total_words++;
      hashline->next  = hashtab->hashtable[hash_val];
      hashline->value = value;
      strcpy(hashline->word, word);
      hashtab->hashtable[hash_val] = hashline;
   }

   /* When _should_ we rehash? */
   if (chain_length >= 1000 ||
	 ( chain_length > 5 &&
	    chain_length * chain_length * chain_length > hashtab->hashsize))
      rehash_table(hashtab);

   return hashline->word;
}

void 
foreach_entry(struct hashtab ** phashtab, hashfunc_p func)
{
   int hashsize, hash_val;
   struct hashentry * hashline, * nexthashline;
   if( *phashtab == 0 || (*phashtab)->hashsize == 0 ) return;

   hashsize = (*phashtab)->hashsize;
   for(hash_val = 0; *phashtab && hash_val <= hashsize; hash_val++) {
      for( hashline = (*phashtab)->hashtable[hash_val];
	   hashline;
	   hashline = nexthashline) {
	 nexthashline = hashline->next;	/* Allow for deletes */
	 func(hashline->word, hashline->value);
      }
   }
}

static int hashvalue(char * word, int hashsize)
{
   int val = 0;
   char *p = word;

   while(*p) {
      val = ((val<<4)^((val>>12)&0xF)^((*p++)&0xFF));
   }
   val &= hashsize;
   return val;
}

static void rehash_table(struct hashtab* hashtab)
{
   struct hashentry ** hashtable = 0;
   struct hashentry * hashline;
   int newhashsize, oldhashsize;
   int oldhash_val, new_hashval;

   oldhashsize = hashtab->hashsize;
   newhashsize = (oldhashsize + 1) * 2 - 1;

   hashtable = calloc((newhashsize+1), sizeof(char*));
   if( hashtable == 0 ) return;

   for(oldhash_val = 0; oldhash_val <= oldhashsize; oldhash_val++) {
      while(hashtab->hashtable[oldhash_val]) {
	 hashline = hashtab->hashtable[oldhash_val];
	 hashtab->hashtable[oldhash_val] = hashline->next;
	 new_hashval = hashvalue(hashline->word, newhashsize);
	 hashline->next = hashtable[new_hashval];
	 hashtable[new_hashval] = hashline;
      }
   }

   free(hashtab->hashtable);
   hashtab->hashtable = hashtable;
   hashtab->hashsize = newhashsize;
}

#ifdef TEST
/* Quick and dirty integer varients; so I can hammer in screws too. :-/  */
void *
find_ientry(struct hashtab ** phashtab, int keyval)
{
   char ibuf[64];
   sprintf(ibuf, "$%x", keyval);
   return find_entry(phashtab, ibuf);
}

char *
set_ientry(struct hashtab ** phashtab, int keyval, void * value)
{
   char ibuf[64];
   sprintf(ibuf, "$%x", keyval);
   return set_entry(phashtab, ibuf, value);
}

void print_str(char * word, void * value)
{
   char * valp = value;
   printf("%s\n", word);
}

int main(int argc, char ** argv)
{
   static char word[256];
   struct hashtab * hashtab[1] = 0;

   while(scanf("%250s", word) == 1)
      set_entry(hashtab, word, &word);

   foreach_entry(hashtab, &print_str);

   fprintf(stderr, "Hash table is size %d with %d words\n", 
	   (*hashtab)->hashsize, (*hashtab)->total_words);
   return 0;
}
#endif
