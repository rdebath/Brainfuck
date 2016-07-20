1i\
#include<unistd.h>\
char M[90000],*m=M;int main(){
s/[^]<>+,-.[]//g
s/+/++*m;/g
s/-/--*m;/g
s/>/++m;/g
s/</--m;/g
s/\[/while(*m){/g
s/]/}/g
s/,/read(0,m,1);/g
s/\./write(1,m,1);/g
$s/$/return 0;}/
