#!/bin/sed -f
# Slurp up the whole program
:slurp
$ !{
    N
    b slurp
}
# Remove non BF
s/[^]<>+,-.[]//g

# Optimisation!!
s/++++/A/g
s/----/B/g
s/\[-]/C/g

# Init the status
s/\(.*\)/:=0000|%:\1:/

:next
s/%\(.\)/\1%/

/%>/ { 
    s/=\([^|]*|\)/\1=/
    s/=:/=0000|:/
    b next
}

/%</ { 
    s/\([^|:]*|\)=/=\1/
    b next
}

/%+/ {
    h
    s/^.*=\(....\).*/\1c/
    t reinc
    :reinc
    s/0c/1/; t incd
    s/1c/2/; t incd
    s/2c/3/; t incd
    s/3c/c0/; t reinc
    s/c//;
    :incd
    G
    s/^\(....\).\(.*\)=\(....\)/\2=\1/
    b next
}

/%-/ {
    h
    s/^.*=\(....\).*/\1c/
    t redec
    :redec
    s/3c/2/; t decd
    s/2c/1/; t decd
    s/1c/0/; t decd
    s/0c/c3/; t redec
    s/c//;
    :decd
    G
    s/^\(....\).\(.*\)=\(....\)/\2=\1/
    b next
}

/%\[/ {
    /=0000/ !b next
    s/^/0/
:skipo
    /^0/ {
	s/%\(.\)/\1%/
	/%\[/ s/^/0/
	/%\]/ s/^0//
	b skipo
    }
    b next
}

/%\]/ {
    /=0000/ b next
    s/^/0/
:skipc
    /^0/ {
	s/\(.\)%/%\1/
	/%\[/ s/^0//
	/%\]/ s/^/0/
	b skipc
    }
    b next
}

/%A/ {
    h
    s/^.*=\(...\)\(.\).*/\1c\2/
    t reinc4
    :reinc4
    s/0c/1/; t incd4
    s/1c/2/; t incd4
    s/2c/3/; t incd4
    s/3c/c0/; t reinc4
    s/c//;
    :incd4
    G
    s/^\(....\).\(.*\)=\(....\)/\2=\1/
    b next
}

/%B/ {
    h
    s/^.*=\(...\)\(.\).*/\1c\2/
    t redec4
    :redec4
    s/3c/2/; t decd4
    s/2c/1/; t decd4
    s/1c/0/; t decd4
    s/0c/c3/; t redec4
    s/c//;
    :decd4
    G
    s/^\(....\).\(.*\)=\(....\)/\2=\1/
    b next
}

/%C/ {
    s/=\(....\)/=0000/
    b next
}

/%\./ { 
    s/=\(....\)\(.*\)/=\1\2\1|/

    /0022|$/ {
	:endprint
	h
	s/^[^:]*:[^:]*:[^:]*://

	# NUL and CR
	s/0000|//g
	s/0031|//g

	# Remove the last NL, "p" will add it back.
	s/0022|$//

	# A few control characters
	s/0021|/	/g
	s/0022|/\
/g
	s/0123|//g

	s/0200|/ /g
	s/0201|/!/g
	s/0202|/"/g
	s/0203|/#/g
	s/0210|/$/g
	s/0211|/%/g
	s/0212|/\&/g
	s/0213|/'/g
	s/0220|/(/g
	s/0221|/)/g
	s/0222|/*/g
	s/0223|/+/g
	s/0230|/,/g
	s/0231|/-/g
	s/0232|/./g
	s/0233|/\//g
	s/0300|/0/g
	s/0301|/1/g
	s/0302|/2/g
	s/0303|/3/g
	s/0310|/4/g
	s/0311|/5/g
	s/0312|/6/g
	s/0313|/7/g
	s/0320|/8/g
	s/0321|/9/g
	s/0322|/:/g
	s/0323|/;/g
	s/0330|/</g
	s/0331|/=/g
	s/0332|/>/g
	s/0333|/?/g
	s/1000|/@/g
	s/1001|/A/g
	s/1002|/B/g
	s/1003|/C/g
	s/1010|/D/g
	s/1011|/E/g
	s/1012|/F/g
	s/1013|/G/g
	s/1020|/H/g
	s/1021|/I/g
	s/1022|/J/g
	s/1023|/K/g
	s/1030|/L/g
	s/1031|/M/g
	s/1032|/N/g
	s/1033|/O/g
	s/1100|/P/g
	s/1101|/Q/g
	s/1102|/R/g
	s/1103|/S/g
	s/1110|/T/g
	s/1111|/U/g
	s/1112|/V/g
	s/1113|/W/g
	s/1120|/X/g
	s/1121|/Y/g
	s/1122|/Z/g
	s/1123|/\[/g
	s/1130|/\\/g
	s/1131|/]/g
	s/1132|/^/g
	s/1133|/_/g
	s/1200|/`/g
	s/1201|/a/g
	s/1202|/b/g
	s/1203|/c/g
	s/1210|/d/g
	s/1211|/e/g
	s/1212|/f/g
	s/1213|/g/g
	s/1220|/h/g
	s/1221|/i/g
	s/1222|/j/g
	s/1223|/k/g
	s/1230|/l/g
	s/1231|/m/g
	s/1232|/n/g
	s/1233|/o/g
	s/1300|/p/g
	s/1301|/q/g
	s/1302|/r/g
	s/1303|/s/g
	s/1310|/t/g
	s/1311|/u/g
	s/1312|/v/g
	s/1313|/w/g
	s/1320|/x/g
	s/1321|/y/g
	s/1322|/z/g
	s/1323|/{/g

	s/1331|/}/g
	s/1332|/~/g
	s/1333|/./g

	# Ignore non-ASCII and most control characters.
	s/0...|//g
	s/[23]...|/./g

	# Do the "|" symbol last.
	s/1330|/|/g

	p
	x
	s/:[^:]*$/:/
    }

    b next
}

/%,/ {
    # This probably is possible, but not yet worthwhile.
    s/$/0022|ERROR0322| Input command not supported0022|/
}

# At the use the existing "." code to convert the last of the printout.
/[0-9][0-9][0-9][0-9]|$/ {
    s/.*:/:%::/
    b endprint
}
d
