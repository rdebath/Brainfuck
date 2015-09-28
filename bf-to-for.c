#include <stdio.h>

/*

 start ==>  {>}m>>+>>+
  >     ==>  >>>>+<<<<<<[>>]>>>>+[-]<<+[<<+>>-]<<-
  <     ==>  +[>>+<<-]>>->>+[<<]>>>>+[-]<<-<<<<
  +     ==>  +[>>+<<-]>>[<+<+>>-]<-<-
  -     ==>  +[>>+<<-]>+>[<-<+>>-]<<-
  [     ==>  +[>>+<<-]>>[>>+<<<<<<[<<]{<}d+{>}d>>[>>]>>-]>>-<<<                  (save control byte)
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             [                                                                   (start actual loop)
  ]     ==>  +[>>>>+<<<<-]>>>>-<<<
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             ]                                                                   (end loop)
             +[<<]{<}d[{>}d>>[>>]<<+[<<]{<}d-]{>}d>>[>>]<<--                     (restore control byte)
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
	    printf("# up\n");
	    break;
	case '-':
	    printf("+[>>+<<-]>+>[<-<+>>-]<<-\n");
	    printf("# down\n");
	    break;
	case '>':
	    printf(">>>>+<<<<<<[>>]>>>>+[-]<<+[<<+>>-]<<-\n");
	    printf("# right\n");
	    break;
	case '<':
	    printf("+[>>+<<-]>>->>+[<<]>>>>+[-]<<-<<<<\n");
	    printf("# left\n");
	    break;

	case '[':
/*

  [     ==>  +[>>+<<-]>>[>>+<<<<<<[<<] {<}d + {>}d >>[>>]>>-]>>-<<<                  (save control byte)
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             [                                                                   (start actual loop)

*/
	    depth++;
	    printf("+[>>+<<-]>>[>>+<<<<<<[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" + ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]>>-]>>-<<<\n");
	    printf("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-<\n");
	    printf("[\n");

	    printf("# begin\n");
	    break;

	case ']':

/*
  ]     ==>  +[>>>>+<<<<-]>>>>-<<<
             +[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-< (compute new control byte)
             ]                                                                   (end loop)
             +[<<] {<}d [ {>}d >>[>>]<<+[<<] {<}d -] {>}d >>[>>]<<--                     (restore control byte)
*/

	    printf("+[>>>>+<<<<-]>>>>-<<<\n");
	    printf("+[>+[<<+>>-]>>+[<<+>>-]>>+<<<<-<<->-]>+[-]>>>>[<<<<<+>>>>>-]<<<<<-<\n");
	    printf("]\n");

	    printf("+[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" [ ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]<<+[<<] ");
	    for(ch=0;ch<depth; ch++) putchar('<');
	    printf(" -] ");
	    for(ch=0;ch<depth; ch++) putchar('>');
	    printf(" >>[>>]<<--\n");

	    depth--;
	    printf("# end\n");
	    break;

	case '.':
	    printf("> %c <\n", ch);
	    printf("# out\n");
	    break;
	case ',':
	    printf("> %c <\n", ch);
	    printf("# in\n");
	    break;
	}
    }
}
