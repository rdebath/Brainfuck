#!/usr/bin/ruby
h = {
    'h' => 'p+=1;',
    'hh' => 'p-=1;',
    'hhh' => 'm[p]+=1;',
    'hhhh' => 'm[p]-=1;',
    'hhhhh' => 'putc m[p];',
    'hhhhhh' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'hhhhhhh' => '(',
    'hhhhhhhh' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);
