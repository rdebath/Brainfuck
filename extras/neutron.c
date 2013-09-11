/* This is the Neutron brainfuck interpreter.
 * It's rather small and dense, but still readable, more or less.
 *
 * Tape size: limited by RAM.
 * Cell size: 8bit with wraparound. 
 * EOF processing: leave as is.
 *
 * Robert de Bath (c) 2013 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bfi { char cmd; struct bfi *next, *jmp; };
struct mem { char val; struct mem *next, *prev; };
int main(int argc, char **argv)
{
    FILE * ifd;
    int ch;
    struct bfi *p=0, *n=0, *j=0, *pgm = 0;
    struct mem *m = calloc(1,sizeof*m);
    setbuf(stdout, NULL);
    if (argc < 2 || strcmp(argv[1], "-") == 0) ifd = stdin;
    else if ((ifd = fopen(argv[1], "r")) == 0) 
	perror(argv[1]);
    if (ifd) {
	while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!')) {
	    if (ch == '<' || ch == '>' || ch == '+' || ch == '-' ||
		ch == '[' || ch == ']' || ch == ',' || ch == '.') {
		if ((n = calloc(1, sizeof*n)) == 0) {
		    perror(argv[0]); exit(1);
		}
		if (p) p->next = n; else pgm = n;
		n->cmd = ch; p = n;
		if (n->cmd == '[') { n->jmp=j; j = n; }
		else if (n->cmd == ']' && j) {
		    n->jmp = j; j = j->jmp; n->jmp->jmp = n;
		}
	    }
	}
	if (ifd!=stdin) fclose(ifd);
	for(n=pgm; n; n=n->next)
	    switch(n->cmd)
	    {
		case '+': m->val++; break;
		case '-': m->val--; break;
		case '.': putchar(m->val); break;
		case ',': if((ch=getchar())!=EOF) m->val=ch; break;
		case '[': if (m->val == 0) n=n->jmp; break;
		case ']': if (m->val != 0) n=n->jmp; break;
		case '<':
		    if (!(m=m->prev)) {
			fprintf(stderr, "%s: Hit start of tape\n", argv[0]);
			exit(1);
		    }
		    break;
		case '>':
		    if (m->next == 0) {
			if ((m->next = calloc(1,sizeof*m)) == 0) {
			    perror(argv[0]); exit(1);
			}
			m->next->prev = m;
		    }
		    m=m->next;
		    break;
	    }
    }
    return !ifd;
}
