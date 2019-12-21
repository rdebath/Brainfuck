#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.downcase.gsub(
	/[a-z][a-z0-9]*|./,
	'gaga' => 'p+=1;',
	'gugu' => 'p-=1;',
	'aaag' => 'm[p]+=1;',
	'uuug' => 'm[p]-=1;',
	'unga' => 'm[p]=0;',
	'guuu' => 'putc m[p];',
	'gaaa' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'gagu' => '(',
	'guga' => ')while((m[p]&=255)!=0);')
