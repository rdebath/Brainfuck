#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read
    .gsub(/Developers|./,Hash.new{|_, k| (k.length>1)?'o':' '; })
    .gsub(/o+|./,
	 'o' => 'm[p]+=1;',
	 'oo' => 'm[p]-=1;',
	 'ooo' => 'p+=1;',
	 'oooo' => 'p-=1;',
	 'ooooo' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	 'oooooo' => 'putc m[p];',
	 'ooooooo' => '(',
	 'oooooooo' => ')while((m[p]&=255)!=0);')
