#!/usr/bin/ruby
h = {
    'zz'   => 'p+=1;',
    '-zz'  => 'p-=1;',
    'z'    => 'm[p]+=1;',
    '-z'   => 'm[p]-=1;',
    'zzz'  => 'putc m[p];',
    '-zzz' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'z+z'  => '(',
    'z-z'  => ')while((m[p]&=255)!=0);',

    'ZZ'   => 'p+=1;',
    '-ZZ'  => 'p-=1;',
    'Z'    => 'm[p]+=1;',
    '-Z'   => 'm[p]-=1;',
    'ZZZ'  => 'putc m[p];',
    '-ZZZ' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'Z+Z'  => '(',
    'Z-Z'  => ')while((m[p]&=255)!=0);',

}
r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(r,h);
