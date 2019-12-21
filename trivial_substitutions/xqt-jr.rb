#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/./,
     '[' => 'm[p]-=1;',
     ']' => 'm[p]+=1;',
     ';' => 'm[p]*=m[p];',
     '.' => 'print m[p].to_s;',
     ',' => 'putc m[p];',
     '@' => 'm[p]=0;',
     '~' => 'print "\033[2J\033[H";',
     '>' => 'p+=1;',
     '<' => 'p-=1;',
     '!' => 'print "the source of your program";',
     )
