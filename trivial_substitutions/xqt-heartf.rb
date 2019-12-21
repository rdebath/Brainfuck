#!/usr/bin/ruby
h = {
    '💗' => 'p+=1;',
    '💜' => 'p-=1;',
    '💖' => 'm[p]+=1;',
    '❤' => 'm[p]-=1;',
    '💌' => 'putc m[p];',
    '❣' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    '💛' => '(',
    '💙' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);

