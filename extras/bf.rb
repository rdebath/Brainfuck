#!/usr/bin/ruby

h = Hash.new{|_, k|
    (k[0] == '+') ? "m[p]+="+k.length.to_s+";\n" :
    (k[0] == '-') ? "m[p]-="+k.length.to_s+";\n" :
    (k[0] == '>') ? "p+="+k.length.to_s+";\n" :
    (k[0] == '<') ? "p-="+k.length.to_s+";\n" :
    (k[0..3] == '[-]+') ? "m[p]="+(k.length-3).to_s+";\n" :
    "#{k}\n" };

h.merge!({
	'>' => 'p+=1;',
	'<' => 'p-=1;',
	'+' => 'm[p]+=1;',
	'-' => 'm[p]-=1;',
	'[' => '(',
	']' => ')while((m[p]&=255)!=0);',
	'.' => 'putc m[p];',
	',' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'[-]' => 'm[p]=0;',
	});

eval 'm=Hash.new(p=0);'+ARGF.read.
    gsub(/[^\[\]+,-.<>]/,'').
    gsub(/\[\-\]\+*|\++|-+|>+|<+|./,h);
