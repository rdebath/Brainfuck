#!/usr/bin/ruby
eval 'c=0;'+ARGF.read.gsub(/./,
        'A' => 'c+=1;',
        'B' => 'c-=1;',
        'C' => 'c=STDIN.getbyte if !STDIN.eof;',
        'D' => 'putc c;')
