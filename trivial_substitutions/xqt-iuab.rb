#!/usr/bin/ruby
h = Hash.new{|_, k| '##'; }.merge({
    'i' => 'p+=1;',
    'use' => 'p-=1;',
    'arch' => 'm[p]+=1;',
    'linux' => 'm[p]-=1;',
    'btw' => 'putc m[p];',
    'by' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'the' => '(',
    'way' => ')while((m[p]&=255)!=0);',
    'gentoo' => 'print "\n",m,"\n";',
    ' ' => ''
});

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/;.*$/,'').gsub(/[\n\t ]+/," ").gsub(r,h).gsub(/.*##.*/,'STDERR.puts "Error in translation\n";');
