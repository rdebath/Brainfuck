
// First count up 2*32 if that's zero we have normal cells
>+ [<++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>-]
  <[>++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++<-]
  >[<++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++>-]
  <[>++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++<-]
  >[<
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    >>+<-]
+< [>>[-<<
     ----------------------------------------------------------------
     ----------------------------------------------------------------
     ----------------------------------------------------------------
     ----------------------------------------------------------------
    >>]<<
    // Weird or huge cells

    [-]>[-]>+<<

]>[
    // Normal binary cells so check how big
    // Test for 2**31 bit cells (exactly 2**32 bits)
    >[ [->
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	>+<<] >[ <<->> >[-<
     ----------------------------------------------------------------
     ----------------------------------------------------------------
	 > ]< [-]] >[-]< <
    [-]]

    >+<<
	// less than 2**32 bits
	[>>-<<
	>

	// Calculate the value 256 and test if it's zero
	++++++++[>++++++++<-]>[<++++>-]
	+<[>-<
	    // Not zero so multiply by 256 again to get 65536
	    [>++++<-]>[<++++++++>-]<[>++++++++<-]
	    +>[>
		// 32 bit
		+
	    <[-]<->] <[>>

		// Check for exactly 16 bit or 9 to 15 bit
		[-]>[-]+[<++++++++>-]<[>++++++++<-]>[<++++++++>-
		]<[>++++++++<-]>[<++++++++>-]+<[[-]>->+<<]

	<<[-]]] >[>
	    // 8 bit or less
	    >>>+<<<
	<[-]]<

	<[-]]

[-]]<

================================================================================
>>


[[-]
// This has either a very large cells or nonbinary ones
// We have to assume that there's some reasonable optimisation
// going on if we want more detail

    // Calculate (16*16*16*16)^128 AKA 2^2048
    ++[>++++++++<-]>[>++++++++<-]>[<<
    +>>[-<<
	[->++++++++++++++++<]>[-<++++++++++++++++>]<
	[->++++++++++++++++<]>[-<++++++++++++++++>]<
    >>]]<+<

    // If that's not zero we have a bignum; probably
    [
	// Calculate 2^115 this is too large for any floating point type
	// to treat as an integer
	[-]>[-]>[-]+++++++<<
	++++++++>>[-<<
	    [->++++++++++++++++<]>[-<++++++++++++++++>]<
	    [->++++++++++++++++<]>[-<++++++++++++++++>]<
	>>]<<

	// If X and X minus one are the same we have a float type
	[->+>+<<] >>- [-<->] +<

	[[-]>

            ++++++++[[->+++++++>+<<]>>[-<<+>>]<[[->++++++>+<<]>>[-<<+>>]<[>>
                >+<+[>-]>[->>+<]<<
            <<-]<-]<-] >>>>[-]>>>[[-]<<<<<<<+>>>>>>>]<<<<<<+<

            [
                // Small and weird cells

		[-]>[-]>[-]>[-]>[-]>[-]>[-]+++++++++++[<++++++<+++<++++<++++++++
		+<++++++++++>>>>>-]<+.<<<++.<--..+++++++.>>>-.<<----.<-.>++++.>>
		.<<<----.+.-.>>+.<---.<-----.+++++.>-.<++++.+++++++.>>>.<+++.--.
		.>>[-]<[-]<[-]<[-]<[-]<

		[-]-

		Print the number
		[[->>+<<]>>[-<++>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[<[-]+>
		->+<[<-]]]]]]]]]]>]<<[>++++++[<++++++++>-]<-.[-]<]]

		>[-]+++++++++[<+++++>-]<+.[-]

            ]>[
                // Huge cells
		>>>>>[-]<[-]<[-]<[-]<[-]<[-]>++++++[<++++>-]<[>+++++>++++>+>++>+
		++<<<<<-]>>>>>.<<<<---.>+++++++.--.>++++++++.<<------.+++.>>.<<-
		---.+.-.>>>---.<<---.+++++++.<.>--------.<++++.+++++++.>>.<++.++
		.<-------------..+++++++.>>.<+.<----.++++++.-------.>--.>>+.>[-]
		<[-]<[-]<[-]<[-]<
            ]<

	<]
	>[

	    // Floating point cells found
	    >>>>[-]<[-]<[-]<[-]<[-]>++++[<++++>-]<[>++++>+++++++>++++++>++<<
	    <<-]>++++++.>----.+++.>+.<+++++.>++++++++.+++++.-------.>.<<----
	    .-.>++.<-.++++++.>>.<------.++.+++++++..<-.>>.<------.<----.++++
	    ++.-------.>--.>++++++++++++++.[-]<[-]<[-]<[-]<

	]<

    <[-]]
    >[>
	// The cell is a binary type less than 2^2048; find exactly how
	// many bits it is
	>+[ [->++<]>[-<+>]<<+> ]<

	// Copy it for the error check
	<[-]>[-<+<+>>]<[->+<]>

	// "This interpreter has "
	>>[-]>[-]>[-]>[-]>[-]<<<++++[<++++>-]<+[>++++++>+++++++>++>++++
	+<<<<-]>>>>-.<<<++.+.>----.>--.<<.+++++.>+.<---------.>--.--.++
	.<.>++.<.>--.>.<<+++.-------.>+.>.>[-]<[-]<[-]<[-]<<<

	// print the number
	>[-]>[-]>[-]>[-]>[-]>[-]+>[-]+>[-]+>[-]+<<<<<<<<<[->>+>+<<<]>>>[
	-<<<+>>>]<[>[-]>[-]<<[>+>+<<-]>>[<<+>>-]+++++++++<[>>>[-]+<<[>+>
	[-]<<-]>[<+>-]>[<<++++++++++>>-]<<-<-]+++++++++>[<->-]<[>+<-]<[>
	+<-]<[>+<-]>>>[<<<+>>>-]>>>[-]<<<+++++++++<[>>>[-]+<<[>+>[-]<<-]
	>[<+>-]>[<<++++++++++>>>+<-]<<-<-]>>>>[<<<<+>>>>-]<<<[-]<<+>]<[>
	[-]<[>+<-]+++++++[<+++++++>-]<-.[-]>>[<<+>>-]<<-]<

	// "bit cells"
	>[-]<[-]++++[->++++++++<]>.[-<+++>]<++.+++++++.+++++++++++.[---
	->+<]>+++.+[-<+++>]<.++.+++++++..+++++++.[----->++<]>.[-]<

	// If less than 33 bits error
	++++[-<++++++++>] <+[-< <+> [-<-> [-<<+>>] ] <<[->>+<<]>> >] <[-]< [[-]
	    >[-]<[-]++++[->++++++++<]>.+[-<++>]<.-.+++.[-->+<]>--.+[-<++>]<
	    +.-[--->+++++<]>+.--.+++.-------.+++.-------.--[---<+>]<...[-]
	]>>

    [-]]<

    [-]++++++++++.

[-]]

================================================================================
>[-<+>]>[-<+>]>[-<+>]>[-<+>]>[-<+>]<<<<<

[[-]
    // 32 bit cells
    // This is Exactly thirty two bits and can be optimised to a printf

    [-]>[-]>[-]>[-]>[-]>[-]>[-]+++++++++++++[<++++++<++++<++<+++++++++<+++++
    +++>>>>>-]<++++++.<<<<.+.>--.>++++++.<<.+++++.>+.<---------.>--.--.++.<.
    >++.<.>--.>.<<+++.-------.>+.>.>-.-.<.<<+.+++++++.>+.>.<<------.++.+++++
    ++..>-.>>----.>>++++++++++.[-]<[-]<[-]<[-]<[-]<[-]<

]

================================================================================
>[-<+>]>[-<+>]>[-<+>]>[-<+>]>[-<+>]<<<<<

================================================================================
>[-<+>]>[-<+>]>[-<+>]>[-<+>]>[-<+>]<<<<<

[[-] // 9 to 31 bits except 16
    // Using a power of two calculation to find the exact cell size

    >+[ [->++<]>[-<+>]<<+> ]<

    // "This interpreter has "
    >>[-]>[-]>[-]>[-]>[-]<<<++++[<++++>-]<+[>++++++>+++++++>++>++++
    +<<<<-]>>>>-.<<<++.+.>----.>--.<<.+++++.>+.<---------.>--.--.++
    .<.>++.<.>--.>.<<+++.-------.>+.>.>[-]<[-]<[-]<[-]<<<

    // print the number
    >[-]>[-]>[-]>[-]>[-]>[-]+>[-]+>[-]+>[-]+<<<<<<<<<[->>+>+<<<]>>>[
    -<<<+>>>]<[>[-]>[-]<<[>+>+<<-]>>[<<+>>-]+++++++++<[>>>[-]+<<[>+>
    [-]<<-]>[<+>-]>[<<++++++++++>>-]<<-<-]+++++++++>[<->-]<[>+<-]<[>
    +<-]<[>+<-]>>>[<<<+>>>-]>>>[-]<<<+++++++++<[>>>[-]+<<[>+>[-]<<-]
    >[<+>-]>[<<++++++++++>>>+<-]<<-<-]>>>>[<<<<+>>>>-]<<<[-]<<+>]<[>
    [-]<[>+<-]+++++++[<+++++++>-]<-.[-]>>[<<+>>-]<<-]<

    // "bit cells"
    >[-]<[-]++++[->++++++++<]>.[-<+++>]<++.+++++++.+++++++++++.[---
    ->+<]>+++.+[-<+++>]<.++.+++++++..+++++++.[----->++<]>.[-]++++++
    ++++.[-]<
]

================================================================================
>[-<+>]>[-<+>]>[-<+>]>[-<+>]>[-<+>]<<<<<

[[-] // 16 bits
    // Also optimised to a printf

    [-]>[-]>[-]>[-]>[-]>[-]>[-]+++++++++++++[<++++++<++++<++<+++++++++<+++++
    +++>>>>>-]<++++++.<<<<.+.>--.>++++++.<<.+++++.>+.<---------.>--.--.++.<.
    >++.<.>--.>.<<+++.-------.>+.>.>---.+++++.<.<<+.+++++++.>+.>.<<------.++
    .+++++++..>-.>>--------.>>++++++++++.[-]<[-]<[-]<[-]<[-]<[-]<

]

================================================================================
>[-<+>]>[-<+>]>[-<+>]>[-<+>]>[-<+>]<<<<<

[   // up to 8 bits

// Check for exactly 8 bits
>[-]<[-]++++[>++++++++<-]>[<++++>-]<[[-]

    // This interpreter has 8 bit cells
    [-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]++++++++++++++[<+<++++++<++++<++<+++++++<
    ++++++++>>>>>>-]<<.<<<++++++.+.<+++.>>++++.<.+++++.<+.>---------.<--.--.
    ++.>.<++.>.<--.>>.<+++.-------.<+.>>.>.<.<+.+++++++.<+.>>.<------.++.+++
    ++++..<-.>>>----------.>>----.[-]<[-]<[-]<[-]<[-]<[-]<

[-]]

// Less than 8 bits
>[-]<[-]++++++++[>++++<-]>[<++++>-]+<[>-<[-]]>[>

    // Find the number
    >+[ [->++<]>[-<+>]<<+> ]<

    // Print the number
    [[->>+<<]>>[-<++>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[<[-]+>
    ->+<[<-]]]]]]]]]]>]<<[>++++++[<++++++++>-]<-.[-]<]]

    ++++[->++++++++<]>.+[-<++>]<.+++++++.+++++++++++.-.[-]++++++++++.[-]

<[-]]<

[-]]

================================================================================
<<
