Justice Suh

cell #0 = 100
++++++++++[>++++++++++<-]>[<+>-]<

cell #2 = sum of 1 to 100
[[>+>+<<-]>>[<<+>>-]<[>>+<<-]<-]>>>[<+>-]<

cell #3 = cell #2
[>+>+<<-]>>[<<+>>-]<

cell #0 = cell #2 * cell #3
[<[<+<+>>-]<[>+<-]>>-]<[-]

cell #2 = 100
++++++++++[>++++++++++<-]>[<+>-]<

loop until cell #2 = 0
[
    cell #4 = cell #3 = cell #2
    [>+>+>+<<<-]>>>[<<<+>>>-]<<

    add cell #3 * cell #4 to cell #1
    [>[>+<<<<+>>>-]>[<+>-]<<-]>[-]<

    <-
]

cell #0 = cell #0 minus cell #1
<[<->-]<

cell #2 = cell #0
[>>+>+<<<-]>>>[<<<+>>>-]<<+>

loop while value exists
[<->[
    divide by ten
    >++++++++++<[->-[>+>>]>[+[-<+>]>+>>]<<<<<]>[-]

    ascii offset
    ++++++++[<++++++>-]

    store remainder
    >[<<+>>-]>[<<+>>-]<<
]>]

make zero
<[->>++++++++[<++++++>-]]

output in reverse order
<[.[-]<]++++++++++.[-]<
