#!/usr/bin/perl
%c=qw(r ;$p+=1 l ;$p-=1 u ;D+=1 d ;D-=1 b ;D&=255;while(D){ e ;D&=255;} o ;print+chrD i ;D=ord(getc) x +1);
$/=$,;
$_=<>;
$_=uc$_;
s/POGACK!/r/g;
s/POGAACK!/l/g;
s/POGAAACK!/u/g;
s/POOCK!/d/g;
s/POGAAACK\?/o/g;
s/POOCK\?/i/g;
s/POGACK\?/b/g;
s/POGAACK\?/e/g;
s/POCK!/x/g;
s/[^rludiobex]//g;
s/./$c{$&}/g;s[D]'$b[$p]'g;
eval
