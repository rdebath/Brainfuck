#!/usr/bin/ruby
h = Hash.new{|_, k|
    if k[0] == "+" then "m[p]+="+k[1..-1]+';'
    elsif k[0] == "-" then "m[p]-="+k[1..-1]+';'
    elsif k[0] == ">" then "p+="+k[1..-1]+';'
    elsif k[0] == "<" then "p-="+k[1..-1]+';'
    end };

h.merge!({
        '>' => 'p+=1;',
        '<' => 'p-=1;',
        '+' => 'm[p]+=1;',
        '-' => 'm[p]-=1;',
        '[' => '(',
        ']' => ')while((m[p]&=255)!=0);',
        '.' => 'putc m[p];',
        ',' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
        '=' => 'm[p]=0;',
        '@' => 'p=0;',
        '!' => 'exit;',
        ':' => 'print (m[p]&255).to_s;',
        });

eval 'm=Hash.new(p=0);'+ARGF.read.gsub(/[-+><][0-9]+|./, h);

