#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * BF translation to BF. This isn't an identity translation because even
 * without most of the peephole optimisation the loader will still remove
 * sequences that are cancelling or that can never be run because they
 * begin with the '][' comment loop introducer.
 */

char bf[] = "><+-.,[]";

char * ook[] = {"Ook. Ook?", "Ook? Ook.", "Ook. Ook.", "Ook! Ook!",
		"Ook! Ook.", "Ook. Ook!", "Ook! Ook?", "Ook? Ook!"};

char *blub[] = {"blub. blub?", "blub? blub.", "blub. blub.", "blub! blub!",
		"blub! blub.", "blub. blub!", "blub! blub?", "blub? blub!"};

char *f__k[] = {"folk", "sing", "barb", "teas", "cask", "kerb", "able", "bait"};

char ** lang = 0;
char txl = 0;
int col = 0;

int state = 0;

int
check_arg(char * arg)
{
    if (strcmp(arg, "-ook") == 0) {
	lang = ook; txl = 0; return 1;
    } else
    if (strcmp(arg, "-blub") == 0) {
	lang = blub; txl = 0; return 1;
    } else
    if (strcmp(arg, "-fk") == 0) {
	lang = f__k; txl = 0; return 1;
    } else
    if (strcmp(arg, "-ris") == 0) {
	lang = 0; txl = 1; return 1;
    } else
    if (strcmp(arg, "-brl") == 0) {
	lang = 0; txl = 2; return 1;
    } else
    if (strcmp(arg, "-spin") == 0) {
	lang = 0; txl = 3; return 1;
    } else
	return 0;
}

static void
pc(int ch)
{
    if (col>=72) {
	putchar('\n');
	col = 0;
    }
    putchar(ch);
    col++;
}

void
outcmd(int ch, int count)
{
    switch(txl) {
    default:
	while(count-->0){
	    if (col>=72) {
		putchar('\n');
		col = 0;
	    }
	    if (lang) {
		col += printf("%s%s", col?" ":"", lang[strchr(bf,ch)-bf]);
	    } else {
		putchar(ch);
		col++;
	    }
	}
	break;

    case 1:
	while(count-->0)
	switch(ch) {
	case '>':
	    if (state!=1) pc('*'); state=1;
	    pc('+');
	    break;
	case '<':
	    if (state!=1) pc('*'); state=1;
	    pc('-');
	    break;
	case '+':
	    if (state!=0) pc('*'); state=0;
	    pc('+');
	    break;
	case '-':
	    if (state!=0) pc('*'); state=0;
	    pc('-');
	    break;
	case '.': pc('/'); pc('/'); break;
	case ',': pc('/'); pc('*'); break;
	case '[': pc('/'); pc('+'); break;
	case ']': pc('/'); pc('-'); break;
	}
	break;

    case 2:
/*
0x01 0x08	    0xe2 0xa0 0x80
0x02 0x10
0x04 0x20
0x40 0x80
*/
	while(count>0) {
	    int c, n, c1, c2;
	    c = strchr(bf,ch)-bf;
	    n = count;
	    if (n > 8) n = 8;

#if 0
	    c1 = (c&3);
	    c2 = 0;
	    if (c>=4) c2 += 1;

	    c1 += ((n&3) << 3);
	    if (n&4) c2 += 2;
	    //c1 += 0x24;

	    pc(0xe2);
	    putchar(0xa0+c2); putchar(0x80+c1);
	    // putchar(0xcc); putchar(0xb6);
#endif
#if 1
	    c2 = 0;
	    c1 = (c&7) + ((n&7) << 3);
	    pc(0xe2);
	    putchar(0xa0+c2); putchar(0x80+c1);
	    // putchar(0xcc); putchar(0xb2);
#endif

	    count -= n;
	}
	break;

    case 3:
	if (ch == '!') state = 7;
	while(count>0) {
	    int sc = 0;
	    while(bf[state] != ch) {
		sc ++;
		state++;
		state &= 7;
	    }
	    pc('0' + sc);
	    state += sc;
	    state &=7;
	    count--;
	}
	break;
    }
    if (ch == '~' && col) {
	putchar('\n');
	col = 0;
    }
}
