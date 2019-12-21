#!/usr/bin/ruby
h = {
    'しの' => 'p+=1;',
    'さばにき' => 'p-=1;',
    'SMD' => 'm[p]+=1;',
    'にゃんこ' => 'm[p]-=1;',
    'ゲェジ' => 'putc m[p];',
    'がりん' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'NEMO' => '(',
    'キュゥ' => ')while((m[p]&=255)!=0);'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);

