#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[!-~]+/,
	'0+' => 'p+=1;',
	'0-' => 'p-=1;',
	'0++' => 'm[p]+=1;',
	'0--' => 'm[p]-=1;',
	'0/' => '(',
	'/0' => ')while(m[p]!=0);',
	'0.' => 'putc m[p];',
	'0?' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'0,' => 'print m[p].to_s;',
	'0;' => 'm[p]=STDIN.gets.to_i if !STDIN.eof;',
	)
