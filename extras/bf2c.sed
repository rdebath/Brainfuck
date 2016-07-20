#!/bin/sed -f
1i\
#include<unistd.h>\
#define r ;m+=1\
#define l ;m-=1\
#define u ;*m+=1\
#define d ;*m-=1\
#define o ;write(1,m,1)\
#define i ;read(0,m,1)\
#define b ;while(*m){\
#define e ;}\
#define _ +1\
#define s ;*m=0\
#define z ;return 0;}\
char mem[30000];int main(){register char*m=mem;
:slurp
$ !{
  N
  b slurp
}
s/[^]<>+,-.[]//g
y/]<>+,-.[/elruidob/
s/\([lrud]\)\1/\1_f\1/g
s/bde/sfu/g
s/f\([lrud]\)\1/_/g
s/f.//g
s/$/z/
s/\(.\)/\1 /g
s/[^\n]\{74\}/&\n/g
s/  *\(\n\|$\)/\1/g
