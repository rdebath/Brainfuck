#!/usr/bin/ruby
h = {
    'b' => 'p+=1;',
    't' => 'p-=1;',
    'pf' => 'm[p]+=1;',
    'bl' => 'm[p]-=1;',
    '!' => 'putc m[p];',
    '?' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    '*gasp*' => '(',
    '*pomf*' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);
