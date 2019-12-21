#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/./,
     'a' => 'p+=1;',
     'c' => 'p-=1;',
     'e' => 'm[p]+=1;',
     'i' => 'm[p]-=1;',
     'j' => 'putc m[p];',
     'o' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
     'p' => '(',
     's' => ')while((m[p]&=255)!=0);')
