#!/usr/bin/ruby
eval'c=1;'+ARGF.read.gsub(/./,'p'=>'c=(c+1)%27;','o'=>'putc c!=0?c+64:32;')+'puts'
