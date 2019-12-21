#!/usr/bin/perl
%c=qw(
    > $p++;
    < $p--;
    + $b[$p]++;
    - $b[$p]--;
    . print+chr$b[$p];
    , $b[$p]=ord(getc);
    [ $b[$p]&=255;while($b[$p]){
    ] $b[$p]&=255;} );
$/=$,;$_=<>;s/./$c{$&}/g;eval
