#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Bourne Shell (old ash) translation from BF, runs at about 170,000 instructions per second.
 *
 * The pseudo RLE is required so that dash doesn't run out of memory on the
 * Lost Kingdom program. (Yes it does work ... but slowly! )
 *
 * NB: Dash needs an MAXPRLE of 2, bash needs 22.
 *
 * It seems that shells older than dash don't implement ${var#?} this is a
 * significant problem for '>' and '<' and probably an insoluble problem
 * for ',' without using external programs.
 *
 * The 'printf' command in the ',' command could be replaced by a "case".
 *
 * If external programs are allowed it becomes a lot simpler.
 *
 * The 'left' and 'right' commands are limited to 100000 cells, however, this
 * limitation is due to performance reasons so this does show the shell is
 * Turing complete even though it doesn't prove it directly.
 */

#define MAXPRLE	25

int do_input = 0;
int do_output = 0;
int ind = 0;

int
check_arg(const char * arg)
{
    if (strcmp(arg, "-b") == 0) return 1;
    if (strcmp(arg, "-no-default-opt") == 0) return 1;
    return 0;
}

void
outcmd(int ch, int count)
{
    int i;
    while (count>MAXPRLE) { outcmd(ch, MAXPRLE); count -= MAXPRLE; }

    if (count > 1) {
	switch(ch) {
	case '+': printf("u%d\n", count); break;
	case '-': printf("d%d\n", count); break;
	case '>': printf("r%d\n", count); break;
	case '<': printf("l%d\n", count); break;
	}
	return;
    }

    switch(ch) {
    case '+': printf("u1\n"); break;
    case '-': printf("d1\n"); break;
    case '>': printf("r1\n"); break;
    case '<': printf("l1\n"); break;
    case '[':
	printf("f1\n");
	printf("while [ \"$A\" != 0 ] ; do\n");
	ind++;
	break;
    case ']':
	printf("f1\n");
	ind--; printf("done\n");
	break;
    case '.': printf("o1\n"); do_output = 1; break;
    case ',': printf("i1\n"); do_input = 1; break;
    }

    switch(ch) {
    case '!':
	printf("#!/bin/sh\n");
	printf("(eval 'x() { :;}' ; x) 2>/dev/null || \n");
	printf("{ echo Shell too old, functions not working. ; exit 1;}\n");
	printf("bf(){\n");
	break;
    case '~':
	printf("}\n");

	if (MAXPRLE>1) {
	    for (i=2; i<=MAXPRLE; i++) {
		printf("u%d() { u1 ; u%d; }\n", i, i-1);
		printf("d%d() { d1 ; d%d; }\n", i, i-1);
		printf("l%d() { l1 ; l%d; }\n", i, i-1);
		printf("r%d() { r1 ; r%d; }\n", i, i-1);
	    }
	}

	printf("u1() { eval \"A=\\$M$P\"; inc; eval \"M$P=$A\" ; }\n");
	printf("d1() { eval \"A=\\$M$P\"; dec; eval \"M$P=$A\" ; }\n");
	printf("f1() { eval \"A=\\$M$P\"; [ .$A = . ] && A=0; }\n");

	printf("\n");
	printf("inc() {\n");
	printf("case \"$A\" in\n");
	for(i=0; i<256; i++)
	    printf("%d ) A=%d ;;\n", i, ((i+1) & 0xFF));
	printf("* ) A=1 ;;\n");
	printf("esac\n");
	printf("}\n");

	printf("\n");
	printf("dec() {\n");
	printf("case \"$A\" in\n");
	for(i=0; i<256; i++)
	    printf("%d ) A=%d ;;\n", i, ((i-1) & 0xFF));
	printf("* ) A=255 ;;\n");
	printf("esac\n");
	printf("}\n");

	printf("\n");
	printf("%s\n",

"\n"	    "r1() {"
"\n"	    "    C=1 ; B= ; P="
"\n"	    "    for v in $PS"
"\n"	    "    do"
"\n"	    "        [ \"$C\" = 1 ] && {"
"\n"	    "            C=0"
"\n"	    "            case \"$v\" in"
"\n"	    "            0) v=1;;"
"\n"	    "            1) v=2;;"
"\n"	    "            2) v=3;;"
"\n"	    "            3) v=4;;"
"\n"	    "            4) v=5;;"
"\n"	    "            5) v=6;;"
"\n"	    "            6) v=7;;"
"\n"	    "            7) v=8;;"
"\n"	    "            8) v=9;;"
"\n"	    "            9) v=0; C=1;;"
"\n"	    "            esac"
"\n"	    "        }"
"\n"	    "        B=\"$B $v\""
"\n"	    "        P=\"$v$P\""
"\n"	    "    done"
"\n"	    "    [ \"$C\" = 1 ] && { B=\"$B 1\" ; P=\"1${P}\" ; }"
"\n"	    "    PS=\"$B\""
"\n"	    "}"
"\n"	    ""
"\n"	    "l1() {"
"\n"	    "    C=1 ; B= ; P= ; LZ= ; LZP="
"\n"	    "    for v in $PS"
"\n"	    "    do"
"\n"	    "        [ \"$C\" = 1 ] && {"
"\n"	    "            C=0"
"\n"	    "            case \"$v\" in"
"\n"	    "            0) v=9;C=1;;"
"\n"	    "            1) v=0;;"
"\n"	    "            2) v=1;;"
"\n"	    "            3) v=2;;"
"\n"	    "            4) v=3;;"
"\n"	    "            5) v=4;;"
"\n"	    "            6) v=5;;"
"\n"	    "            7) v=6;;"
"\n"	    "            8) v=7;;"
"\n"	    "            9) v=8;;"
"\n"	    "            esac"
"\n"	    "        }"
"\n"	    "        if [ \"$v\" = 0 ]"
"\n"	    "        then LZ=\"$LZ $v\""
"\n"	    "             LZP=\"$v$LZP\""
"\n"	    "        else B=\"$B$LZ $v\""
"\n"	    "             P=\"$v$LZP$P\""
"\n"	    "             LZP= ; LZ="
"\n"	    "        fi"
"\n"	    "    done"
"\n"	    "    PS=\"$B\""
"\n"	    "}"
	    );

	if(do_output) {
	    printf("\n");
	    printf("o1() {\n");
	    printf("eval \"A=\\$M$P\"\n");
	    printf("case \"$A\" in\n");
	    for(i=0; i<256; i++) {
		if (i >= ' ' && i <= '~' && i != '\'' && i != '\\')
		    printf("%d ) echon '%c' ;;\n", i, i);
		else if (i == 10 )
		    printf("%d ) echo ;;\n", i);
		else if (i < 64)
		    printf("%d ) echoe '\\%03o' ;;\n", i, i);
		else
		    printf("%d ) echoe '\\%04o' ;;\n", i, i);
	    }
	    printf("esac\n}\n");
	    printf("%s\n",
"\n"		"if [ .`echo -n` = .-n ]"
"\n"		"then"
"\n"		"    echon() { echo \"$1\\c\"; }"
"\n"		"    echoe() { echo \"$1\\c\"; }"
"\n"		"else"
"\n"		"    echon() { echo -n \"$1\"; }"
"\n"		"    if [ .`echo -e` = .-e ]"
"\n"		"    then echoe() { echo -n \"$1\"; }"
"\n"		"    else echoe() { echo -n -e \"$1\"; }"
"\n"		"    fi"
"\n"		"fi"
		);
	}

	if (do_input)
	    printf("\n%s\n%s\n%s\n%s\n%s\n",
		"# Use other tools here, input is not needed for TC",
		"i1() {",
		"A=`dd bs=1 count=1 2>/dev/null | od -t d1 | awk '{print $2;}'`",
	        "eval \"M$P=$A\"",
		"}");


	printf("\nP=\n");
	printf("# Everything should be ready so do a final sanity check\n");
	printf("(r2;u2;r1;u1;l1;f1;[ \"$A\" = 2 ]) 2>/dev/null ||");
	printf("{ echo Broken shell, more insane than normal. >&2; exit 1;}\n");
	printf("bf\n");
	break;
    }
}

#if 0

 RLE on the +/- commands is possible (below) but it takes bash 15 seconds to
 create the addition table. Running 65536 direct assignments is quicker,
 but still very slow.

p=255
for i in \
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 \
27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 \
51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 \
75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 \
99 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 \
117 118 119 120 121 122 123 124 125 126 127 128 129 130 131 132 133 134 \
135 136 137 138 139 140 141 142 143 144 145 146 147 148 149 150 151 152 \
153 154 155 156 157 158 159 160 161 162 163 164 165 166 167 168 169 170 \
171 172 173 174 175 176 177 178 179 180 181 182 183 184 185 186 187 188 \
189 190 191 192 193 194 195 196 197 198 199 200 201 202 203 204 205 206 \
207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 222 223 224 \
225 226 227 228 229 230 231 232 233 234 235 236 237 238 239 240 241 242 \
243 244 245 246 247 248 249 250 251 252 253 254 255
do eval "inc$p=$i" ; p=$i
done

sum=0
a=0
b=0
while :
do  while :
    do  eval "sum${a}_${b}=$sum"
	eval "sum=\$inc$sum"
	eval "a=\$inc$a"
	[ "$a" = 0 ] && break
    done
    eval "sum=\$inc$sum"
    eval "b=\$inc$b"
    [ "$b" = 0 ] && break
done

echo $sum15_250
// ############################################################


#endif
