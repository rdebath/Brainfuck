#!/usr/bin/ruby
h = Hash.new{|_, k| 
    if k =~ /^lol(ol)*$/ then "m[p]+="+((k.length-1)/2).to_s+";"
    elsif k =~ /^rii*p$/ then "m[p]-="+(k.length-2).to_s+";"
    elsif k[0..5] == "shift " then "p+="+k[5..-1]+";"
    end };

h.merge!({
        'chris' => 'p+=1;',
        'lee' => 'p-=1;',
        'lol' => 'm[p]+=1;',
        'rip' => 'm[p]-=1;',
        'chris?' => '(',
        'lee?' => ')while((m[p]&=255)!=0);',
        'ok' => 'putc m[p];',
        'pls' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
        'rekt' => 'm[p]=0;',
        'gg' => 'exit;',
        'troll' => 'print (m[p]&255).to_s;',
        });

eval 'm=Hash.new(p=0);'+ARGF.read.downcase.gsub(/[\n\t ]+/," ")
    .gsub(/shift -?[0-9]+|[!-~]+|./, h);
