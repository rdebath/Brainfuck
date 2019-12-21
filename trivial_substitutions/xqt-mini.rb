#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/./,
     'r' => 'p+=1;',
     'l' => 'p-=1;',
     'u' => 'm[p]+=1;',
     'd' => 'm[p]-=1;',
     'o' => 'putc m[p];',
     'i' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
     'b' => '(',
     'e' => ')while((m[p]&=255)!=0);')
