1i\
#include<unistd.h>\
#define r m+=1\
#define l m-=1\
#define u ++*m\
#define d --*m\
#define z *m=0\
#define o write(1,m,1)\
#define i read(0,m,1)\
#define b while(*m)\
#define _ return 0;}\
char mem[30000];int main(){register char*m=mem;\

s/[^]<>+,-.[]//g
s/\[-]/z;/g
s/+/u;/g
s/-/d;/g
s/\./o;/g
s/,/i;/g
s/\[/b{/g
s/\]/}/g
s/>/r;/g
s/</l;/g
   
$a\
_
