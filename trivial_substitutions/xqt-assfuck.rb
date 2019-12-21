#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[Aa][Ss][Ss]|./,
     'asS' => 'p-=1;',
     'aSs' => 'p+=1;',
     'Ass' => 'm[p]-=1;',
     'ass' => 'm[p]+=1;',
     'AsS' => '(',
     'ASS' => ')while((m[p]&=255)!=0);',
     'ASs' => 'putc m[p];',
     'aSS' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
