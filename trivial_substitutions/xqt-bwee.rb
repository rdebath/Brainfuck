#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[!-~]+|./,
	'BWEE' => 'p+=1;',
	'BWEEE' => 'p-=1;',
	'BWEEEE' => 'm[p]+=1;',
	'BWEEEEE' => 'm[p]-=1;',
	'BWEEEEEE' => 'putc m[p];',
	'BWEEEEEEE' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'BWEEEEEEEE' => '(',
	'BWEEEEEEEEE' => ')while((m[p]&=255)!=0);')
