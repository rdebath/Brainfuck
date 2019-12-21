#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[-><][-><]|./,
	'->' => 'p+=1;',
	'<-' => 'p-=1;',
	'><' => 'm[p]+=1;',
	'<>' => 'm[p]-=1;',
	'<<' => 'putc m[p];',
	'>>' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'-<' => '(',
	'>-' => ')while((m[p]&=255)!=0);',
	'--' => 'print p,",",m,"\n";')

#   >>  input a byte to current cell
#   <<  output the current cell as character
#   ->  move memory pointer one right
#   <-  move memory pointer one left
#   ><  increase the current cell
#   <>  decrease the current cell
#   -<  if the current cell is zero, jump to the corresponding >-, otherwise continue
#   >-  jump back to the corresponding -<
#   --  output the current state of the storage tape
