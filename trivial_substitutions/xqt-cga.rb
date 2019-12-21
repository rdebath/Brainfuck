#!/usr/bin/ruby

h = {
    '文明' => 'p+=1;',
    '和谐' => 'p-=1;',
    '爱国' => 'm[p]+=1;',
    '自由' => 'm[p]-=1;',
    '诚信' => 'putc m[p];',
    '友善' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    '富强' => '(',
    '民主' => ')while((m[p]&=255)!=0);',
    '敬业' => 'm[p]+=5;',
    '平等' => 'm[p]-=5;'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(r,h);
