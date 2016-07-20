#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[^adehkKoOru]/,'').gsub(
	/(kO|Ko|[Kk]u(dah|karek|d))/,
	'Kudah' => 'p+=1;',
	'kudah' => 'p-=1;',
	'Ko' => 'm[p]+=1;',
	'kO' => 'm[p]-=1;',
	'Kud' => '(',
	'kud' => ')while((m[p]&=255)!=0);',
	'Kukarek' => 'putc m[p];',
	'kukarek' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
