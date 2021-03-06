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
 * If external programs are allowed it becomes a lot simpler.
 */

#define MAXPRLE	25

static int do_input = 0;
static int do_output = 0;
static int ind = 0;

static int do_f1 = 0, do_arr = 0;
static int max_u = 0, max_d = 0, max_l = 0, max_r = 0;
#define setmax(max_x, val) if(val>max_x) max_x = val;

static void print_string(char * str);

static gen_code_t gen_code;
struct be_interface_s be_interface = {
    .gen_code=gen_code,
    .bytesonly=1, .disable_be_optim=1, .enable_chrtok=1};

static void
gen_code(int ch, int count, char * strn)
{
    int i;

    if (ch == '=')
	count &= 255;
    else {

	while (count>MAXPRLE) { gen_code(ch, MAXPRLE, 0); count -= MAXPRLE; }

	if (count > 1) {
	    switch(ch) {
	    case '+': printf("u%d\n", count); setmax(max_u, count); break;
	    case '-': printf("d%d\n", count); setmax(max_d, count); break;
	    case '>': printf("r%d\n", count); setmax(max_r, count); break;
	    case '<': printf("l%d\n", count); setmax(max_l, count); break;
	    }
	    return;
	}
    }

    switch(ch) {
    case '"': print_string(strn); break;
    case '=': printf("eval \"M$P=%d\"\n", count); do_arr = 1; break;
    case '+': printf("u1\n"); setmax(max_u, 1); break;
    case '-': printf("d1\n"); setmax(max_d, 1); break;
    case '>': printf("r1\n"); setmax(max_r, 1); break;
    case '<': printf("l1\n"); setmax(max_l, 1); break;
    case '[':
	printf("f1\n"); do_f1 = 1;
	printf("while [ \"$A\" != 0 ] ; do\n");
	ind++;
	break;
    case ']':
	printf("f1\n"); do_f1 = 1;
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
	    printf("\n");
	    for (i=2; i<=MAXPRLE; i++) {
		if (i <= max_u)
		    printf("u%d() { u%d ; u%d; }\n", i, i-i/2, i/2);
		if (i <= max_d)
		    printf("d%d() { d%d ; d%d; }\n", i, i-i/2, i/2);
		if (i <= max_l)
		    printf("l%d() { l%d ; l%d; }\n", i, i-i/2, i/2);
		if (i <= max_r)
		    printf("r%d() { r%d ; r%d; }\n", i, i-i/2, i/2);
	    }
	}

	if (max_u>0) {
	    do_arr = 1;
	    printf("\n");
	    for(i=0; i<256; i++)
		printf("inc_%d=%d\n", i, ((i+1) & 0xFF));
	    printf("inc_=1\n");
	    printf("\n");
	    printf("u1() { eval \"A=\\$M$P\"; eval \"M$P=\\$inc_$A\"; }\n");
	}

	if (max_d>0) {
	    do_arr = 1;
	    printf("\n");
	    for(i=0; i<256; i++)
		printf("dec_%d=%d\n", i, ((i-1) & 0xFF));
	    printf("dec_=255\n");
	    printf("\n");
	    printf("d1() { eval \"A=\\$M$P\"; eval \"M$P=\\$dec_$A\"; }\n");
	}

	if (do_f1) {
	    do_arr = 1;
	    printf("\n");
	    printf("f1() { eval \"A=\\$M$P\"; [ \".$A\" = . ] && A=0; }\n");
	}

	if (max_r>0 || max_l>0)
	    do_arr = 1;

	if (max_r>0) printf("%s\n",

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
	    );

	if (max_l>0) printf("%s\n",
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

	    printf("%s\n",
"\n"		"# shellcheck disable=SC2006,SC2039"
"\n"		"if [ \".`echo -n`\" = .-n ]"
"\n"		"then"
"\n"		"    echon() { echo \"$1\\c\"; }"
"\n"		"    echoe() { echo \"$1\\c\"; }"
"\n"		"else"
"\n"		"    echon() { echo -n \"$1\"; }"
"\n"		"    if [ \".`echo -e`\" = .-e ]"
"\n"		"    then echoe() { echo -n \"$1\"; }"
"\n"		"    else echoe() { echo -n -e \"$1\"; }"
"\n"		"    fi"
"\n"		"fi"
		);
	    printf("\n");

	    do_arr = 1;
	    printf("o1() {\n");
	    printf("eval \"A=\\$M$P\"\n");
	    printf("o2 $A\n");
	    printf("}\n");
	    printf("\n");

            printf("if [ \".$(echoe '\\171')\" = .y ]\n");
            printf("then\n\n");

	    printf("o2() {\n");
	    printf("case \"$1\" in\n");
	    for(i=0; i<256; i++) {
		if (i >= ' ' && i <= '~' && i != '\'' && i != '\\' && i != '-')
		    printf("%d ) echon '%c' ;;\n", i, i);
		else if (i == 10 )
		    printf("%d ) echo ;;\n", i);
		else
		    printf("%d ) echoe '\\%03o' ;;\n", i, i);
	    }
	    printf("esac\n}\n");
            printf("else\n\n");

	    printf("o2() {\n");
	    printf("case \"$1\" in\n");
	    for(i=0; i<256; i++) {
		if (i >= ' ' && i <= '~' && i != '\'' && i != '\\' && i != '-')
		    printf("%d ) echon '%c' ;;\n", i, i);
		else if (i == 10 )
		    printf("%d ) echo ;;\n", i);
		else if (i < 64)
		    printf("%d ) echoe '\\%03o' ;;\n", i, i);
		else
		    printf("%d ) echoe '\\%04o' ;;\n", i, i);
	    }
	    printf("esac\n}\n");

            printf("fi\n\n");
	}

	if (do_input) {
	    do_arr = 1;
	    printf("\n%s\n%s\n%s\n%s\n%s\n\n",
		"# Use other tools here, input is not needed for TC",
		"i1() {",
		"A=`dd bs=1 count=1 2>/dev/null | od -t d1 | awk '{print $2;}'`",
	        "[ \"$A\" != '' ] && eval \"M$P=$A\"",
		"}");
	}

	if (do_arr)
	    printf("P=\n");

	if (max_r>=2 && max_l>=1 && max_u>=2 && max_l>=1 && do_f1) {
	    printf("# Everything should be ready so do a final sanity check\n");
	    printf("(r2;u2;r1;u1;l1;f1;[ \"$A\" = 2 ]) 2>/dev/null ||");
	    printf("{ echo Broken shell, more insane than normal. >&2; exit 1;}\n");
	}

	printf("bf\n");
	break;
    }
}

static void
print_string(char * str)
{
    char buf[256];
    int gotnl = 0, badchar = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || badchar || outlen > sizeof(buf)-8))
	{
	    if (gotnl) {
		buf[--outlen] = 0;
		if (outlen == 0) {
		    printf("echo\n");
		} else {
		    printf("echo '%s'\n", buf);
		}
	    } else {
		buf[outlen] = 0;
		printf("echon '%s'\n", buf);
		do_output = 1;
	    }
	    gotnl = 0; outlen = 0;
	}
	if (badchar) {
	    printf("o2 %d\n", badchar);
	    badchar = 0;
	    do_output = 1;
	}
	if (!*str) break;

	if (*str == '\n') {
	    gotnl = 1;
	    buf[outlen++] = *str;
	} else if (*str >= ' ' && *str <= '~' && *str != '\'' && *str != '\\' && *str != '-') {
	    buf[outlen++] = *str;
	} else {
	    badchar = (*str & 0xFF);
	}
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
