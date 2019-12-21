#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[wlWL][oO][oO]|./,
	'woo' => 'p+=1;',
	'loo' => 'p-=1;',
	'Woo' => 'm[p]+=1;',
	'Loo' => 'm[p]-=1;',
	'wOO' => 'putc m[p];',
	'lOO' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'WOO' => '(',
	'LOO' => ')while((m[p]&=255)!=0);')
