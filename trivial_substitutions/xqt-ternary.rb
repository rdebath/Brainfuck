#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[012][012]|./,
	'01' => 'p+=1;',
	'00' => 'p-=1;',
	'11' => 'm[p]+=1;',
	'10' => 'm[p]-=1;',
	'20' => 'putc m[p];',
	'21' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'02' => '(',
	'12' => ')while((m[p]&=255)!=0);',
	'22' => 'print p,",",m,"\n";')
