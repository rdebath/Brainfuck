: '['
# This regression test triggers bugs in the esotope-bfc, libbf
# and bfdb compilers.
#
# Robert de Bath 2019

for b in 8 16 32
do echo "Esotope $b:" "$( tcc -w -run <( esotope-bfc -s$b $0 ) )"
done

echo "libbf char: $(libbf --compile-execute-char  $0 2>/dev/null)"
echo "libbf short: $(libbf --compile-execute-short  $0 2>/dev/null)"
echo "libbf int: $(libbf --compile-execute-int  $0 2>/dev/null)"

echo "bfdb int: $(ulimit -t 3; bfdb -n -u $0 2>/dev/null)"

for b in 8 16 32 ; do echo "Tritium $b: $(bfi -b$b $0)" ; done

exit 0
<<\END
: ']'

// This code runs on byte sized cells
[-]>[-]++++++++[<++++++++>-]<[>++++<-]+>[<->[-]]<[[-]
+++++++++[->+++++++++<]>++.+[---<++++>]<-.+++..+++++++.-[---->+<]>++.---
[-<++++>]<.------------.+.++++++++++.+[---->+<]>+++.[--<+++++++>]<.++.--
-.--------.+++++++++++.>++++[<---->-]<-.++++++++++++.-[--->+<]>----.++++
[-<+++>]<++.---------..-.+++++++++++++++.+[---->+<]>+++.[-<+++>]<+.-[---
>+<]>.++++[-<+++>]<.-----------.+>++++[<++++>-]<.-----------.--.++++++++
+++++.[--->+<]>------.+[-<+++>]<.++.+++++++..[--->+<]>----.---[-<++++>]<
-.----------.+>++++[<++++>-]<.>+++++++[<--->-]<.[-]++++++++++.
[-]]

// This generates 256 to check for larger than byte cells
>[-]<[-]++++++++[>++++++++<-]>[<++++>-]<[[-]
// Check that the cells size is okay
+>++++[-<[->>++<<]>>[-<<++>>]<]+<[>-<[-]]>[ [-]+++++++. [><]]<

// The payload code:
++++++++[->+++++++++<]>.----[--<+++>]<-.+++++++..+++.[--->+<]>-----.---[
-<+++>]<.---[--->++++<]>-.+++.------.--------.-[---<+>]<.[--->+<]>-.[-]<
// to here

[-]]
