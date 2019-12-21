#!/usr/bin/ruby
print ARGF.read.gsub(/[^\[\]<>+,-.]/,'').gsub(/(.{1,72})/, "\\1\n");
