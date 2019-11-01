[ This is a VERY simple check for the three common bit sizes.
  It's the original version for the Bitwidth.b test program.

  Robert de Bath 2013
]

// Calculate the value 256 and test if it's zero
// If the interpreter errors on overflow this is where it'll happen
++++++++[>++++++++<-]>[<++++>-]
+<[>-<
    // Not zero so multiply by 256 again to get 65536
    [>++++<-]>[<++++++++>-]<[>++++++++<-]
    +>[>
        // Print "32"
        ++++++++++[>+++++<-]>+.-.[-]<
    <[-]<->] <[>>
        // Print "16"
        +++++++[>+++++++<-]>.+++++.[-]<
<<-]] >[>
    // Print "8"
    ++++++++[>+++++++<-]>.[-]<
<-]<
// Print " bit cells\n"
+++++++++++[>+++>+++++++++>+++++++++>+<<<<-]>-.>-.+++++++.+++++++++++.<.
>>.++.+++++++..<-.>>-
Clean up used cells.
[[-]<]
