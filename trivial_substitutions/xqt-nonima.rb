#!/usr/bin/ruby
# Language 'nonimatulaelcebe'
eval 'm=Hash.new(p=0);'+ARGF.read.downcase.gsub(
	/ma|la|tu|ni|be|ce|no|el|./,
	'ma' => 'p+=1;',
	'la' => 'p-=1;',
	'tu' => 'm[p]+=1;',
	'ni' => 'm[p]-=1;',
	'be' => 'putc m[p];',
	'ce' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'no' => '(',
	'el' => ')while((m[p]&=255)!=0);')
