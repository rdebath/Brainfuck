#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/8=+D|./,
    '8=D' => 'p+=1;',
    '8==D' => 'p-=1;',
    '8===D' => 'm[p]+=1;',
    '8====D' => 'm[p]-=1;',
    '8=====D' => 'putc m[p];',
    '-' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    '8======D' => '(',
    '8=======D' => ')while((m[p]&=255)!=0);'
);

