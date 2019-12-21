#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.downcase.gsub(
	/[a-z][a-z0-9]*|./,
	'right' => 'p+=1;',
	'left' => 'p-=1;',
	'up' => 'm[p]+=1;',
	'down' => 'm[p]-=1;',
	'out' => 'putc m[p];',
	'in' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'begin' => '(',
	'end' => ')while((m[p]&=255)!=0);')
