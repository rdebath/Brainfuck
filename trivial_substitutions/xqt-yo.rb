#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(
	/[Yy][oO][!?]?|./,
	'yo' => 'p+=1;',
	'YO' => 'p-=1;',
	'Yo!' => 'm[p]+=1;',
	'Yo?' => 'm[p]-=1;',
	'YO!' => 'putc m[p];',
	'yo?' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
	'yo!' => '(',
	'YO?' => ')while((m[p]&=255)!=0);')

# yo    increment the data pointer (to point to the next cell to the right).
# YO    decrement the data pointer (to point to the next cell to the left).
# Yo!   increment (increase by one) the byte at the data pointer.
# Yo?   decrement (decrease by one) the byte at the data pointer.
# YO!   output the byte at the data pointer.
# yo?   accept one byte of input, storing its value in the byte at the data pointer.
# yo!   if the byte at the data pointer is zero, then instead of moving
#       the instruction pointer forward to the next command, jump it
#       forward to the command after the matching YO? command.
# YO?   if the byte at the data pointer is nonzero, then instead of
#       moving the instruction pointer forward to the next command,
#       jump it back to the command after the matching yo! command.
