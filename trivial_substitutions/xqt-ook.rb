#!/usr/bin/ruby
print 'm=Hash.new(p=0);'+ARGF.read.downcase.gsub(/[0-9]/,'_').
	gsub(/([!-~]*)/, ' \1 ').gsub(/[\n\t ]+/,' ').
	gsub(/ ook/,' 3').gsub(/\./,'0').gsub(/\!/,'1').gsub(/\?/,'2').
	gsub(/3([0-2]) 3([0-2])/, '3\1\2');
	gsub(/[!-~]*/,
	'302' => 'p+=1;',
	'320' => 'p-=1;',
	'300' => 'm[p]+=1;',
	'311' => 'm[p]-=1;',
	'312' => '(',
	'321' => ')while((m[p]&=255)!=0);',
	'310' => 'putc m[p];',
	'301' => 'm[p]=STDIN.getbyte if !STDIN.eof;')
