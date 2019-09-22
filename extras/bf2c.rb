#!/usr/bin/ruby
# This is a slightly optimising BF to C translation. The generated C is then
# compiled, loaded as a shared library and executed.
require "inline"
class BFCode
    inline do |builder|
	h = Hash.new{|_, k| 
	    (k[0] == "+") ? "m[p]+="+k.length.to_s+";\n" :
	    (k[0] == "-") ? "m[p]-="+k.length.to_s+";\n" :
	    (k[0] == ">") ? "p+="+k.length.to_s+";\n" :
	    (k[0] == "<") ? "p-="+k.length.to_s+";\n" :
	    (k[0..3] == "[-]+") ? "m[p]="+(k.length-3).to_s+";\n" :
	    "#{k}\n" }.merge({
		'[-]' => "m[p]=0;\n",
		'>' => "p++;\n",
		'<' => "p--;\n",
		'+' => "m[p]++;\n",
		'-' => "m[p]--;\n",
		'[' => "while(m[p]!=0){\n",
		']' => "}\n",
		'.' => "putchar(m[p]);\n",
		',' => "m[p]=getchar();\n"})

	bfcode ="void bfprog() {\n" +
		"static unsigned char m[100000];\n" +
		"register int p=0;\n" +
		"setbuf(stdout, 0);\n" +
		ARGF.read.
		gsub(/[^\[\]+,-.<>]/,'').
		gsub(/\[\-\]\+*|\++|-+|>+|<+|./,h) +
		"}\n";

	builder.include "<stdio.h>"
	# Beware, I've put a hash of the generated code into the "prefix"
	# because the main argument to the "c" method is NOT part of the
	# cache key.
	builder.prefix "/*#{ Digest::MD5.hexdigest(bfcode) }*/"
	builder.c bfcode
    end
end

BFCode.new().bfprog();
