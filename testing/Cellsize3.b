[   This test is used to determine the cell size for the interpreter.
    It can only detect the three common sizes, others will (probably)
    be detected as 32 bit unless the cell size is quite small, in which
    case the program will output a zero.  The calculations used to get
    256 and 65536 are a little more complex than they should need to be
    to defeat the optimisations of certain interpreters that incorrectly
    wrap cells at compile time.
]
// This generates 256 to check for larger than byte cells
+>>++++[-<<[->++++<]>[-<+>]>]< + <[[-]>[-]<

    // This checks for ANY cell size under 1331 by using a double byte
    // increment and checking the high 'byte'
    >>>>>
    >+++++++++++[<+++++++++++>-]<[-<+++++++++++[-<
	<+>+[<-]<[-<<+>]>>
    >]>]<<[-]<<
    +<[[-]>-<]
    +>

    [[-]

	// This generates 65536 to check for larger than 16bit cells
	++>>+++++[-<<[->++++++++<]>[-<+>]>]< + <[[-]>[-]>

	    // 32
	    [-]++++++++++[>+++++<-]>+.-.[-]<

	// ELSE
	<[-]<[-] ] >[>

	    // 16
	    +++++++[>+++++++<-]>.+++++.[-]<
	<[-]]<


    <[-]>[-]]<[-
	// 0
	>>[-]>[-]++++++++[<++++++>-]<.[-]<<
    [-]]


// ELSE
>[-]<[-]] >[[-]<

    // 8
    ++++++++[>+++++++<-]>.[-]<

>[-]]<
++++++++++.[-]
