#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[!-~]+|./,
	'meeP' => 'p+=1;',
	'Meep' => 'p-=1;',
	'mEEp' => 'm[p]+=1;',
	'MeeP' => 'm[p]-=1;',
	'MEEP' => 'putc m[p];',
	'meep' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'mEEP' => '(',
	'MEEp' => ')while((m[p]&=255)!=0);')
