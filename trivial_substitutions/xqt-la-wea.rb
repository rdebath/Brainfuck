#!/usr/bin/ruby
h = {
    'puta' => 'p+=1;',
    'chucha' => 'p-=1;',
    'weón' => 'm[p]+=1;',
    'aweonao' => 'm[p]+=2;',
    'maricón' => 'm[p]-=1;',
    'maraco' => 'm[p]-=2;',
    'maraca' => 'm[p]=0;',
    'ctm' => 'putc m[p];',
    'quéweá' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
    'pichula' => '(',
    'tula' => ')while(m[p]!=0);',
    'pico' => 'break;',
    'chúpala' => 'print m[p].to_s;',
    'brígido' => 'm[p] = Integer(gets) rescue 0;',
    'mierda' => 'exit;',
    'perkin' => 'if pf then pf=false; m[p]=v; else pf=true; v=m[p]; end;'
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);pf=false;'+ARGF.read.downcase.gsub(/[\n\t ]+/," ").gsub(r,h);
