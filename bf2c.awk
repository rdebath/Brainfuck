#!/bin/sh -
# BF to C converter, compiler and runner.
#
# This converts BF to peephole optimised C then compiles it with the system C
# complier and runs the resulting executable.
#
# This script works with most versions of awk including the one described in
# the book "The AWK Programming Language", by Al Aho, Brian Kernighan,
# and Peter Weinberger. BUT for versions that don't accept -Wexec any
# options must use '+' instead of '-' or add a "--" option after the script
# name and before those options.
#
# The 'mawk' version is the quickest followed by "original-awk" with the big
# fat GNU version rolling up eventually.
#
# Use the shell to fix the lack of -Wexec on some awks.
true , /; exec awk -f "$0" -- "$@" ; exit; / {}
#
BEGIN {
    if (ARGC == 1) {
	print "Usage: bf2c [options] files..."
	print " Options:"
	print "  -c        Don't run the program, send it to stdout."
	print "  -O -O3    Pass this option to 'cc'."
	print "  -O0       Disable peephole optimisations."
	print "  -#        Dump final memory and when a # is seen."
	print "  -i        Enable counting of BF instructions."
	print "  -M65536   Specify amount of memory to allocate."
	print "  -Tint     Type for the tape cells."
	print "  -Ctcc     Use the specified C compiler rather than 'cc'."
	print ""
	print "Note: the -i and -# options interfere with optimisation."
	aborting=1;
	exit
    }

    memory = 65536;
    memoff = 1000;
    outs = "";
    CC = "cc";
    noopt = 0
    ll = 78;

    for(i=1; i<ARGC; i++) {
	arg = ARGV[i];
	if (substr(arg,1,1) == "+") arg = "-" substr(arg,2);
	if (arg == "-c") { ARGV[i] = ""; norun=1 }
	if (arg == "-#") { ARGV[i] = ""; debug=1 }
	if (arg == "-i") { ARGV[i] = ""; icount=1 }
	if (arg == "-O0") { ARGV[i] = ""; noopt=1 }
	if (substr(arg,1,2) == "-M") { memory = substr(ARGV[i], 3) + memoff; ARGV[i] = "";}
	if (substr(arg,1,2) == "-T") { cell = substr(ARGV[i], 3); ARGV[i] = "";}
	if (substr(arg,1,2) == "-C") { CC = substr(ARGV[i], 3); ARGV[i] = "";}
	if (substr(arg,1,2) == "-O") { optflag = " -O" substr(ARGV[i], 3); ARGV[i] = "";}
    }

    header();

    if (icount) tc = "t "; else tc = "";
    for (i=1; i<=8; i++) tx[substr("<>+-.,[]", i, 1)] = tc substr("lrudoibE", i, 1) " ";
    if (debug) tx["#"] = "# ";
}

{
    s = ""
    for(i=1; i<=length($0); i++) {
        c = substr($0,i,1);
	s = s tx[c];
    }
    str = str s;

    while (index(str, "E")) {
	if (!noopt) {
	    gsub("b d E", "z", str);
	    if (icount) {
		gsub("t b t d t E", "t T(*m*2)z", str);
	    }
	}

	e = index(str, "E");
	tstr = substr(str, e+2);
	str = substr(str, 1, e+1);
	optim();
	e = index(str, "E");
	if (e) {
	    lstr = lstr substr(str, 1, e+1);
	    str = substr(str, e+2);
	    print_line();
	}
	str = str tstr;
    }
}

END {
    if (aborting) exit;

    optim();
    lstr = lstr str; str = "";
    if (debug) lstr = lstr "# ";
    ll = 1;
    print_line();
    if (icount && !debug) {
	print "fprintf(stderr, \"\\nCommands executed: %.0Lf\\n\", (long double)icount);" > of
    }
    print "return 0;}" > of

    if (!norun) {
	close(of);
	rv = system(CC optflag " -o " ef " " of);
	system("rm " of);
	if (rv == 0) {
	    rv = system(ef " +deleteme+");
	    if (rv != 0) printf "Command returned exit status: %d\n", rv > "/dev/stderr"
	}
	if (rv != 0)
	    system("rm -f " ef);
    }
}

function header() {
    if (norun) of = "/dev/stdout"; else {
	ef="/tmp/ap" rand();
	of = ef ".c"
    }

    if (cell != "") tcell=cell; else tcell="unsigned char";

    print "#include <stdlib.h>" > of
    print "#include <unistd.h>" > of
    print "#include <stdio.h>"  > of
    print "#define r m+=1;" > of
    print "#define l m-=1;" > of
    print "#define u *m+=1;"    > of
    print "#define d *m-=1;"    > of
    if (cell != "" ) {
	print "#define o putch(*m);" > of
	print "#define i *m = getch(*m);" > of
    } else {
	print "#define o write(1,m,1);" > of
	print "#define i read(0,m,1);"  > of
    }
    if (noopt) {
	print "#define b while(*m){"    > of
	print "#define e }"             > of
    } else {
	print "#define b while(*m)"     > of
	print "#define z *m=0;" > of
	print "#define S(x) *m= x;"     > of
	print "#define R(x) m += x;"    > of
	print "#define L(x) R(-x)"      > of
	print "#define U(x) *m += x;"   > of
	print "#define D(x) U(-x)"      > of
	print "#define Q(x,y) if(*m)m[x]=(y);"  > of
	print "#define M(x,y) m[x]+=(*m)*(y);"  > of
	print "#define A(x) m[x]+=(*m);"        > of
    }
    print "#define C",tcell         > of
    tcell="C"
    if (icount) {
	print "#define t icount++;" > of
	print "#define T(x) icount += (x);" > of
	print "#ifdef __x86_64" > of
	print "__int128 icount = 0;" > of
	print "#else" > of
	print "long long icount = 0;" > of
	print "#endif" > of
    }

    if (cell != "") {
	print "void putch(int ch) { putchar(ch); }" > of;
	print "int getch(int och) { " \
		"int ch; " \
		"if((ch=getchar()) != EOF) return ch; else return och; " \
		"}" > of;
    }

    print tcell,"mem[" memory "];" \
	"int main(int argc, char ** argv){" \
	"register",tcell "*m=mem+" memoff ";"    > of

    if (oflush || cell != "" )
	print "setbuf(stdout,0);" > of;
    if (!norun) {
	printf "if (argc>1 && strcmp(argv[1], \"+deleteme+\") == 0)" > of;
	printf " unlink(argv[0]);\n" > of;
    }

}

function print_line() {
    if (noopt) {
	gsub("E", "e", lstr);
    } else {
	gsub("[Bb] ", "b{", lstr);
	gsub("E", "}", lstr);
	gsub(" *} *", "}", lstr);
    }

    for(;;) {
	if (length(lstr) < ll) return;

	s = substr(lstr, 1, 79);
	if ((i= index(s, "#")) != 0) {
	    print substr(lstr, 1, i-1) > of
	    lstr = substr(lstr, i+1);
	    debug_mem();
	} else if (match(s, ".* "))  {
	    print substr(s, 1, RLENGTH-1) > of
	    lstr = substr(lstr, RLENGTH+1);
	} else {
	    print lstr > of
	    lstr = "";
	}
    }
}

function debug_mem() {
    if (icount) {
	print "fprintf(stderr, \"\\nCommands executed: %.0Lf\\n\", (long double)icount);" > of
    }
    print "{ int ii, jj=0;" > of;
    print "  for(ii=" memoff "; ii<sizeof(mem)/sizeof(*mem); ii++)" > of;
    print "    if(mem[ii]) jj = ii+1;" > of;
    print "  fprintf(stderr, \"Ptr: %2d: mem:\", m-mem-" memoff ");" > of;
    print "  for(ii=" memoff "; ii<jj || mem+ii<=m; ii++)" > of;
    print "    fprintf(stderr, \" %3d\", mem[ii]);" > of;
    print "  fprintf(stderr, \"\\n\");" > of;
    print "  if (mem-m-" memoff "<100) {" > of;
    print "    fprintf(stderr, \"         ptr:\", m-mem-" memoff ");" > of;
    print "    for(ii=" memoff "; mem+ii<=m; ii++)" > of;
    print "      fprintf(stderr, \"   %s\", mem+ii==m?\"^\":\" \");" > of;
    print "    fprintf(stderr, \"\\n\");" > of;
    print "  }" > of;
    print "}" > of;
}

function optim()
{
    if (noopt) return;
    if (icount) {
	while (match(str, "t (t|u|d|l|r|i|o| )*")) {
	    v = substr(str, RSTART, RLENGTH)
	    v2 = v;
	    gsub("[^t]", "", v2);
	    v2 = "T(" length(v2) ")"
	    gsub("[t ]", "", v);
	    gsub(".", "& ", v);
	    str = substr(str, 1, RSTART-1) v v2 substr(str, RSTART+RLENGTH)
	}
	gsub("T(1)", "t ", str);
    }

    while ( match(str, "u d ") || match(str, "d u ") ||
	    match(str, "l r ") || match(str, "r l ")) {
	str = substr(str, 1, RSTART-1) substr(str, RSTART+RLENGTH)
    }

    while (match(str, "b (z|u|d|l|r| )* E")) {
	v = substr(str, RSTART, RLENGTH)

	for(i in a) delete a[i];
	for(i in z) delete z[i];
	off = 0;
	c = split(v, p);
	for(i=1; i<=c; i++) {
	    if (p[i] == "r"){off++; continue; }
	    if (p[i] == "l"){off--; continue; }
	    if (p[i] == "u"){a[off] ++; continue; }
	    if (p[i] == "d"){a[off] --; continue; }
	    if (p[i] == "z"){z[off] = 1; a[off] = 0; }
	}
	if (z[0]==1 || a[0] != -1 || off != 0) {
	    str = substr(str, 1, RSTART-1) "B" substr(str, RSTART+1)
	    continue
	}
	nv = "";
	for(i in a) if(i != 0) {
	    if (z[i]) {
		nv = nv sprintf("Q(%d,%d)", i, a[i]);
	    } else if (a[i] == 1) {
		nv = nv sprintf("A(%d)", i);
	    } else {
		nv = nv sprintf("M(%d,%d)", i, a[i]);
	    }
	}
	nv = nv "z"

	str = substr(str, 1, RSTART-1) nv substr(str, RSTART+RLENGTH)
	continue
    }

    while (match(str, "z u (u )*")) {
	str = substr(str, 1, RSTART-1) "S(" RLENGTH/2-1 ")" substr(str, RSTART+RLENGTH)
    }

    while (match(str, "u u (u )*")) {
	str = substr(str, 1, RSTART-1) "U(" RLENGTH/2 ")" substr(str, RSTART+RLENGTH)
    }

    while (match(str, "d d (d )*")) {
	str = substr(str, 1, RSTART-1) "D(" RLENGTH/2 ")" substr(str, RSTART+RLENGTH)
    }

    while (match(str, "r r (r )*")) {
	str = substr(str, 1, RSTART-1) "R(" RLENGTH/2 ")" substr(str, RSTART+RLENGTH)
    }

    while (match(str, "l l (l )*")) {
	str = substr(str, 1, RSTART-1) "L(" RLENGTH/2 ")" substr(str, RSTART+RLENGTH)
    }
}
# vim: set filetype=awk:
