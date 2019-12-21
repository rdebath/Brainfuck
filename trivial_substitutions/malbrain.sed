1i\
#include<unistd.h>\
#define b while(*m) {\
#define i read(0,m,1);\
#define o write(1,m,1);\
#define d --*m;\
#define u ++*m;\
#define l m-=1;\
#define r m+=1;\
#define e }\
#define _ return 0;}\
char mem[30000];int main(){register char*m=mem;
$a\
_
s/[^>.]//g
y/>./NE/
H
g
s/\n//g
s/^N/l/
s/^E/rE/
t nxtl
:nxtl
s/^rN/l/
s/^lN/u/
s/^uN/d/
s/^dN/o/
s/^oN/i/
s/^iN/b/
s/^bN/e/
s/^eN/r/
t nxtl
h
s/^\(.\)E.*/\1/
t prti
b dele
:prti
p
g
s/^\(.\)E/\1/
t nxtl
:dele
d
