#include <stdio.h>

/*

start ==>  {>}m>>+>>+
  >     ==>  >>>>+<<<<<<[>>]>>>>+[-]<<+[<<+>>-]<<-
  <     ==>  +[>>+<<-]>>->>+[<<]>>>>+[-]<<-<<<<
  +     ==>  +[>>+<<-]>>[<+<+>>-]<-<-
  -     ==>  +[>>+<<-]>+>[<-<+>>-]<<-
  [     ==>  +[>>+<<-]>>[>>+<<<<[<<]{<}d+{>}d>>[>>]>>-]>>-<<<               (save control byte)
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             [                                                              (start actual loop)
  ]     ==>  +[>>>>+<<<<-]>>>>-<<<
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             ]                                                              (end loop)
             +[-]+[<<]{<}d[{>}d>>[>>]<<+[<<]{<}d-]{>}d>>[>>]<<--            (restore control byte)

*/

int
main(int argc, char ** argv)
{
    int ch;
    int depth = 0;
    int maxdepth = 4;

    for(ch=0; ch<maxdepth; ch++)
	putchar('>');
    printf(" >>>> >>+>>+\n");
    while((ch = getchar()) != EOF) {
	switch(ch) {
	case '+':
	    printf("+[>>+<<-]>>[<+<+>>-]<-<-\n");
	    printf("#\n");
	    break;
	case '-':
	    printf("+[>>+<<-]>+>[<-<+>>-]<<-\n");
	    printf("#\n");
	    break;
	case '>':
	    printf(">>>>+<<<<<<[>>]>>>>+[-]<<+[<<+>>-]<<-\n");
	    printf("#\n");
	    break;
	case '<':
	    printf("+[>>+<<-]>>->>+[<<]>>>>+[-]<<-<<<<\n");
	    printf("#\n");
	    break;

	case '[':
/*
  [     ==>  +[>>+<<-]>>[>>+<<<<[<<] {<}d + {>}d >>[>>]>>-]>>-<<<               (save control byte)
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             [                                                              (start actual loop)

*/
	    depth++;
	    printf("+[>>+<<-]>>[>>+<<<<[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" + ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]>>-]>>-<<<\n");
	    printf("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-<\n");
	    printf("[\n");

	    printf("#\n");
	    break;

	case ']':

/*
  ]     ==>  +[>>>>+<<<<-]>>>>-<<<
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             ]                                                              (end loop)
             +[-]+[<<] {<}d [ {>}d >>[>>]<<+[<<] {<}d -] {>}d >>[>>]<<--            (restore control byte)
*/

	    printf("+[>>>>+<<<<-]>>>>-<<<\n");
	    printf("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->]>>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)\n");
	    printf("]                                                              (end loop)\n");

	    printf("+[-]+[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" [ ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]<<+[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" -] ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]<<--            (restore control byte)\n");

	    depth--;
	    printf("#\n");
	    break;
	case '.':
	case ',':
	    printf("> %c <\n", ch);
	    printf("#\n");
	    break;
	}
    }
}
