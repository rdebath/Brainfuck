#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read
    .gsub(/Developer[sz]|./i,Hash.new{|_, k| (k.length>1)?k[-1]:' '; })
    .gsub(/[sz]+|./,
	 's' => 'm[p]+=1;',
	 'ss' => 'm[p]-=1;',
	 'sss' => 'p+=1;',
	 'ssss' => 'p-=1;',
	 'sssss' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	 'ssssss' => 'putc m[p];',
	 'sssssss' => '(',
	 'ssssssss' => ')while((m[p]&=255)!=0);',
	 'z' => 'sleep(1);',
	 'zz' => 'print "\033[H\033[J";',
	 'zzz' => 'm[p]=(rand() * 256).to_i;',
	 )
