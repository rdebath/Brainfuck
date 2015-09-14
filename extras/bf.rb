#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/./,
	'>' => 'p+=1;',
	'<' => 'p-=1;',
	'+' => 'm[p]+=1;',
	'-' => 'm[p]-=1;',
	'[' => '(',
	']' => ')while((m[p]&=255)!=0);',
	'.' => 'putc m[p];',
	',' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
