#!/usr/bin/ruby
h = {
    "It's alright" => 'p-=1;',
    'turn me on, dead man' => 'p+=1;',
    'Number 9' => 'm[p]+=1;',
    'if you become naked' => 'm[p]-=1;',
    'The Beatles' => 'putc m[p];',
    'Paul is dead' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'Revolution 1' => '(',
    'Revolution 9' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);
