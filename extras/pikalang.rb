#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/([A-Za-z_][A-Za-z0-9_]*)|./,
	'pipi' => 'p+=1;',
	'pichu' => 'p-=1;',
	'pi' => 'm[p]+=1;',
	'ka' => 'm[p]-=1;',
	'pika' => '(',
	'chu' => ')while((m[p]&=255)!=0);',
	'pikachu' => 'putc m[p];',
	'pikapi' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
