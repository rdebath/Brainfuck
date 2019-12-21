#!/usr/bin/ruby

s = 0;
h = Hash.new{|_, k| 
    if k == "'" then s=1-s ; ""
    elsif k == '"' && s==0 then "("
    elsif k == '"' && s!=0 then ")while((m[p]&=255)!=0);"
    else ""
    end };
h[';'] = "p+=1;"
h['-'] = "p-=1;"
h['.'] = "m[p]+=1;"
h[','] = "m[p]-=1;"
h['!'] = 'putc m[p];'
h['?'] = 'm[p]=STDIN.getbyte if !STDIN.eof;'

eval 'm=Hash.new(p=0);' + ARGF.read.gsub(/./,h)
