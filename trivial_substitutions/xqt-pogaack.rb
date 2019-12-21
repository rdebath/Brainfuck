#!/usr/bin/ruby
eval 'm=Hash.new(p=0);'+ARGF.read.upcase.
    gsub(/PO[GOC][ACK][A-Z]*[!\?]/,
	'POGACK!' => 'r',
	'POGAACK!' => 'l',
	'POGAAACK!' => 'u',
	'POOCK!' => 'd',
	'POGAAACK?' => 'o',
	'POOCK?' => 'i',
	'POGACK?' => 'b',
	'POGAACK?' => 'e',
	'POCK!' => 'x').
    gsub(/[^rludoibex]/,'').
    gsub(/[rlud]x*|./,
	Hash.new{|_, k|
	(k[0] == 'u') ? "m[p]+="+k.length.to_s+";\n" :
	(k[0] == 'd') ? "m[p]-="+k.length.to_s+";\n" :
	(k[0] == 'r') ? "p+="+k.length.to_s+";\n" :
	(k[0] == 'l') ? "p-="+k.length.to_s+";\n" :
	"#{k}\n"
      }.merge({
	'r' => 'p+=1;',
	'l' => 'p-=1;',
	'u' => 'm[p]+=1;',
	'd' => 'm[p]-=1;',
	'b' => '(',
	'e' => ')while((m[p]&=255)!=0);',
	'o' => 'putc m[p];',
	'i' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
      }));
