#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/#[^\n]*\n|./,
    '<' => 'p-=1;',
    '>' => 'p+=1;',
    '-' => 'm[p]-=1;',
    '+' => 'm[p]+=1;',
    '[' => '(',
    ']' => ')while((m[p]&=255)!=0);',
    'o' => 'putc m[p];',
    'i' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
