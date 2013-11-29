[+++++++.+++.][   <- If you get this nl your loops are fucked. ->

    This routine is a demonstration of checking for the three cell sizes
    that are normal for Brainfuck. The demo code also checks for several
    bugs that have been noted in optimising and non-optimising interpreters
    and compilers.

    It should print one of three slight variations of "Hello world" followed
    by an exclamation point, the maximum cell value (if it's less than a
    few thousand) and a newline.

    If the interpreter is broken in some way it can print a lot of other
    different strings and frequently causes the interpreter to crash.

    It does work correctly with 'bignum' cells.
]
>>

	This code runs at pointer offset two and unknown bit width; don't
	assume you have more that eight bits

	======= DEMO CODE =======
	First just print "Hello"

	Notice that I reset the cells despite knowing that they are zero
	this is a test for proper functioning of the ability to skip over
	a loop that's never executed but isn't actually a comment loop

	Also there's some commented out code afterwards

	[-]>[-]++++++++[<+++++++++>-]<.>+++++[<+++++>-]<++++.
	+++++++..+++.
	[-] [[-]++>[-]+++++[<++++++>-]<.++>+++++++[<+++++++>-]<.+>+
	+++[<+++++>-]<.+.+++++++++++.------------.---.----.+++.>++++
	++++[<-------->-]<---.>++++[<----->-]<---.[-][]]

	===== END DEMO CODE =====
<<

Calculate the value 256 and test if it's zero
If the interpreter errors on overflow this is where it'll happen
++++++++[>++++++++<-]>[<++++>-]
+<[>-<
Multiply by 256 again to get 65536
[>++++<-]>[<++++++++>-]<[>++++++++<-]
+>[>
	Cells should be 32bits at this point

	The pointer is at cell two and you can continue your code confident
	that there are big cells

	======= DEMO CODE =======
	This code rechecks that the test cells are in fact nonzero
	If the compiler notices the above is constant but doesn't
	properly wrap the values this will generate an incorrect
	string

	Print a message
	++>[-]++++++[<+++++++>-]<.------------.[-]
	<[>+<[-]]>
	++++++++>[-]++++++++++[<+++++++++++>-]<.--------.+++.------.
	--------.[-]

	===== END DEMO CODE =====

<[-]<[-]>] <[>>
	Cells should be 16bits at this point

	The pointer is at cell two and you can continue your code confident
	that there are medium sized cells; if you use a BF doubler you should
	have the initial 'right' but don't have to

	======= DEMO CODE =======
	Space
	++>[-]+++++[<++++++>-]<.[-]

	I'm rechecking that the cells are 16 bits
	this condition should always be true

	+>>++++[-<<[->++++<]>[-<+>]>]< + <[ >>

	    Print a message
	    >[-]++++++++++[<+++++++++++>-]<+++++++++.--------.
	    +++.------.--------.[-]

	<[-]<[-] ] >[> > Dead code here
	    This should never be executed because it's in an 8bit zone hidden
	    within a 16bit zone; a really good compiler should delete this
	    If you see this message you have dead code walking

	    Print a message
	    [-]>[-]+++++++++[<++++++++++>-]<.
	    >++++[<+++++>-]<+.--.-----------.+++++++.----.
	    [-]

	<<[-]]<
	===== END DEMO CODE =====

<<[-]] >[-]< ] >[>
	Cells should be 8bits at this point

	The pointer is at cell two but you only have 8 bits cells
	and it's time to use the really bit and slow BF quad encoding

	======= DEMO CODE =======
	Space
	++>[-]+++++[<++++++>-]<.[-]

        Another dead code check
        [-]>[-]>[-]<++[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-
        ]<[<++++++++>-]<[[-]>[-]+++++++++[<++++++++++>-]<.>++++[<+++++>-]<+.
        --.-----------.+++++++.----.>>[-]<+++++[>++++++<-]>++.<<[-]]

	Print a message
	[-] <[>+<[-]]> +++++>[-]+++++++++[<+++++++++>-]<.
	>++++[<++++++>-]<.+++.------.--------.
	[-]
	===== END DEMO CODE =====

<[-]]<

+[[>]<-]    Check unbalanced loops are ok

>>
	======= DEMO CODE =======
	Back out and print the last two characters

	This loop is a bug check for handling of nested loops; it goes
	round the outer loop twice and the inner loop is skipped on the
	first pass but run on the second

	BTW: It's unlikely that an optimiser will notice how this works

	>
	    +[>[
		Print the exclamation point
		[-]+++>[-]+++++[<++++++>-]<.

	    <[-]>[-]]+<]
	<

	This part finds the actual value that the cell wraps at; even
	if it's not one of the standard ones; but we only count up to
	a few thousand: any higher and we print nothing

	This has a reasonably deep nested loop and a couple of loops
	that have unbalanced pointer movements

	Find maxint (if small)
	[-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<++++[->>++++>>++++>>++
	++<<<<<<]++++++++++++++>>>>+>>++<<<<<<[->>[->+>[->+>[->+>+[>
	>>+<<]>>[-<<+>]<-[<<<[-]<<[-]<<[-]<<[-]>>>[-]>>[-]>>[-]>->+]
	<<<]>[-<+>]<<<]>[-<+>]<<<]>[-<+>]<<<]>+>[[-]<->]<[->>>>>>>[-
	<<<<<<<<+>>>>>>>>]<<<<<<<]<

	The number is only printed if we found the actual maxint
	[
	    Space
	    >[-]>[-]+++++[<++++++>-]<++.[-]<

	    Print the number
	    >>>[-]>[-]>[-]>[-]>[-]>[-]+>[-]+>[-]+>[-]+<<<<<<<<<<<[->>>>+
	    >+<<<<<]>>>>>[-<<<<<+>>>>>]<[>[-]>[-]<<[>+>+<<-]>>[<<+>>-]++
	    +++++++<[>>>[-]+<<[>+>[-]<<-]>[<+>-]>[<<++++++++++>>-]<<-<-]
	    +++++++++>[<->-]<[>+<-]<[>+<-]<[>+<-]>>>[<<<+>>>-]>>>[-]<<<+
	    ++++++++<[>>>[-]+<<[>+>[-]<<-]>[<+>-]>[<<++++++++++>>>+<-]<<
	    -<-]>>>>[<<<<+>>>>-]<<<[-]<<+>]<[>[-]<[>+<-]+++++++[<+++++++
	    >-]<-.[-]>>[<<+>>-]<<-]<<<

	[-]]

	One last thing; an exclamation point is not a valid BF instruction!

	Print the newline
	[-]++++++++++.[-]
	[
	    Oh, and now that I can use "!" the string you see should be one of:
	    Hello World! 255
	    Hello world! 65535
	    Hello, world!

	    And it should be followed by a newline.
	]

	===== END DEMO CODE =====
<<
