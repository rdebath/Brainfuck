#!/usr/bin/ruby

h = {
    'praytoshishkirka' => 'm[p]+=1;',
    'praytopolinka' => 'm[p]-=1;',
    'praytoshshkrkX10' => 'm[p]+=10;',
    'praytoplnkX10' => 'm[p]-=10;',
    '<-' => 'p-=1;',
    '->' => 'p+=1;',
    'shishpol' => 'print m[p].to_s;',
    'shishkirism' => 'putc m[p];',
    'shishkirkapolinka' => 'm[p]=0;',
    'shshclr' => 'print "\033[38;5;" + m[p].to_s + "m";',
    'plnk!' => 'sleep(m[p]/1000.0);',
    'shshkrkplnk' => 'p=0;',
    'shshpofck' => 'raise "666";',
    'polshsh' => 'print "\n";',

#   '' => 'm[p]=STDIN.getbyte if !STDIN.eof;',
#   '' => '(',
#   '' => ')while((m[p]&=255)!=0);',
}

r = Regexp.union(Regexp.union(h.keys.sort{|a,b|b.length<=>a.length}),/./);
eval 'm=Hash.new(p=0);'+ARGF.read.gsub(r,h);

## praytoshishkirka	accumulator + 1
## praytopolinka	accumulator - 1
## praytoshshkrkX10	accumulator + 10
## praytoplnkX10	accumulator - 10
## <-	pointer move left
## ->	pointer move right
## shishpol	displays the value of the variable
## shishkirism	displays the value of the variable symbol
## shishkirkapolinka	reset accumulator
## shshclr	changes the color of the console text by changing the cell value
## plnk!	creates a delay in the program due to the value of the variable in milliseconds
## shshkrkplnk	reset cell
## shshpofck	generates a program error with code 666
## polshsh	creates a new line
