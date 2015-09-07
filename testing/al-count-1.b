[ This is a binary counter benchmark program, it runs
671105767 instructions and is mostly unoptimisable.
As it runs it prints the letters of the alphabet with
a delay that doubles between each letter.           ]

>++++++++[>+++>>>++++++++<<<<-]>++<<
#
+[>		    // The loop flag
    >>[>>]          // Find first zero
    <[		    // Check for the term flag
	[->>+<<]    // if found move it up one
	>>
    >+.[->>+<<]<

	-[<<]	    // Decrement and finish on zero
    ]>              // Back to the zero
    +               // Make the zero a one
    <<[-<<]         // Remove the ones ending before the LSB
<]                  // and loop
>+>>[-<<]<-	    // Clean up
#
// Print "OK\n"
++++++++++.
#
