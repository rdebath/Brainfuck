#!/usr/bin/ruby
h = {
    '💗' => '>',
    '💜' => '<',
    '💖' => '+',
    '❤' => '-',
    '💌' => '.',
    '❣️' => ',',
    '💛' => '[',
    '💙' => ']'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
print 'm=Hash.new(p=0);'+ARGF.read.gsub(/[\n\t ]+/," ").gsub(r,h);

