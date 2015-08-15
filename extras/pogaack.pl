#!/usr/bin/perl
%c=qw(r $p++ l $p-- u D++ d D-- b D&=255;while(D){ e D&=255;} o print+chrD i D=ord(getc));
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
s/[^rludiobe]//g;
s/./$c{$&};/g;
s[D]'$b[$p]'g;
eval
