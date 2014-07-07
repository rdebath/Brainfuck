/* This is the deadbeef brainfuck interpreter.
 *
 * Robert de Bath (c) 2014 GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void run(void);
struct bfi { int mov; int cmd; int arg; } *pgm = 0;
int pgmlen = 0, on_eof = 1, debug = 0;

int main(int argc, char **argv)
{
   FILE * ifd;
   int ch;
   int p= -1, n= -1, j= -1;
   for (;;) {
      if (argc < 2 || argv[1][0] != '-' || argv[1][1] == '\0') {
	 break;
      } else if (!strcmp(argv[1], "-e")) { argc--; argv++; on_eof = -1;
      } else if (!strcmp(argv[1], "-z")) { argc--; argv++; on_eof = 0;
      } else if (!strcmp(argv[1], "-n")) { argc--; argv++; on_eof = 1;
      } else if (!strcmp(argv[1], "-d")) { argc--; argv++; debug = 1;
      } else if (argv[1][0] == '-') {
	 fprintf(stderr, "Unknown option '%s'\n", argv[1]);
	 exit(1);
      } else break;
   }
   ifd = argc>1&&strcmp(argv[1],"-")?fopen(argv[1],"r"):stdin;
   if(!ifd) perror(argv[1]); else {
      while((ch = getc(ifd)) != EOF && (ifd!=stdin || ch != '!' || j>=0)) {
	 int r = (ch == '<' || ch == '>' || ch == '+' || ch == '-');
	 if (r || (debug && ch == '#') || (ch == ']' && j>=0) ||
	       ch == '[' || ch == ',' || ch == '.') {
	    if (ch == '<') {ch = '>'; r = -r;}
	    if (ch == '-') {ch = '+'; r = -r;}
	    if (r && p>=0 && pgm[p].cmd == ch) { pgm[p].arg += r; continue; }
	    if (p>=0 && pgm[p].cmd == '=' && ch == '+')
		{ pgm[p].arg += r; continue; }
	    if (p>=0 && pgm[p].cmd == '>') { pgm[p].mov = pgm[p].arg; }
	    else {
	       n++;
	       if (n>= pgmlen-2) pgm = realloc(pgm, (pgmlen=n+99)*sizeof*pgm);
	       if (!pgm) { perror("realloc"); exit(1); }
	       pgm[n].mov = 0;
	    }
	    pgm[n].cmd = ch; pgm[n].arg = r; p = n;
	    if (pgm[n].cmd == '[') { pgm[n].arg=j; j = n; }
	    else if (pgm[n].cmd == ']') {
	       pgm[n].arg = j; j = pgm[j].arg; pgm[pgm[n].arg].arg = n;
	       if (  pgm[n].mov == 0 && pgm[n-1].mov == 0 &&
		     pgm[n-1].cmd == '+' && (pgm[n-1].arg&1) == 1 &&
		     pgm[n-2].cmd == '[') {
		  n -= 2; pgm[p=n].cmd = '='; pgm[n].arg = 0;
	       } else if (pgm[n-1].cmd == '[') {
		  n--; pgm[p=n].cmd = '?'; pgm[n].arg = pgm[n+1].mov;
	       }
	    }
	 }
      }
      while(j>=0) { p=j; j=pgm[j].arg; pgm[p].arg=0; pgm[p].cmd = '+'; }
      if (ifd!=stdin) fclose(ifd);
      setbuf(stdout, NULL);
      if (pgm) { pgm[n+1].cmd = 0; run(); }
   }
   return !ifd;
}

void run(void)
{
static unsigned char t[USHRT_MAX+1];
   unsigned short m = 0;
   int n, ch;
   for(n=0; ; n++) {
      m += pgm[n].mov;
      switch(pgm[n].cmd)
      {
	 case 0:    return;
	 case '=':  t[m] = pgm[n].arg; break;
	 case '+':  t[m] += pgm[n].arg; break;
	 case '[':  if (t[m] == 0) n=pgm[n].arg; break;
	 case ']':  if (t[m] != 0) n=pgm[n].arg; break;
	 case '?':  while(t[m]) m += pgm[n].arg; break;
	 case '>':  m += pgm[n].arg; break;
	 case '.':  putchar(t[m]); break;
	 case ',':  if((ch=getchar())!=EOF) t[m]=ch;
		    else if (on_eof != 1) t[m]=on_eof;
		    break;
	 case '#':
	    fprintf(stderr, "\n%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n%*s\n",
	       t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7],t[8],t[9],4*m+3,"^");
	    break;
      }
   }
}
