#!/usr/bin/ruby
h = {
    'hold your horses now' => 'p+=1;',
    'sleep until the sun goes down' => 'p-=1;',
    'through the woods we ran' => 'm[p]+=1;',
    'deep into the mountain sound' => 'm[p]-=1;',
    "don't listen to a word i say" => 'putc m[p];',
    'the screams all sound the same' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'though the truth may vary' => '(',
    'this ship will carry' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);
