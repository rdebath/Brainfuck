#!/usr/bin/ruby
eval 'p=0;f=1;'+ARGF.read.gsub(/./,
        ':' => 'p=0;f=1;',
        '^' => 'p^=f;',
        '<' => 'f*=2;',
        '!' => 'putc p;')
