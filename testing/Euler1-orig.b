[///////////////////////////////////////////////////////////////////////////////

Project Euler Problem 1: Solution by: James Stanley
https://github.com/jes/bnbf
zLib license

$ bcci-wrap Euler1.b
Project Euler .net Problem 1

Multiples of 3 and 5
If we list all the natural numbers below 10 that are multiples of 3 or 5,
we get 3, 5, 6 and 9. The sum of these multiples is 23.

Find the sum of all the multiples of 3 or 5 below 1000.
...

233168
$

///////////////////////////////////////////////////////////////////////////////]
Project Euler Problem 1 by James Stanley

Generate 1000
++++++++++[>++++++++++[>++++++++++<-]<-]>>
Take it down to 995
-----

Memory now: === 0 0 (995) result=0 ===

repeatedly add the number at 995 to our result and then take 5 off 995

[[->+>+<<] >>[-<<+>>]<< -----]

Move back to address 0
<<

Memory now: === (0) 0 0 result=99500 ===

Generate 1000
++++++++++[>++++++++++[>++++++++++<-]<-]>>
Take it down to 999
-

Memory now: === 0 0 (999) result=99500 ===

for 3's we do the same as the loop above for 5's but remember to ignore the
ones that were counted for 5 and basically unroll the loop and only count it on
the ones that aren't divisible by 15
also we quit looping when the outer loop should end

[
  <[-]<[-]>> [-<+<+>>] <<[->>+<<]>> < [[->>+>+<<<] >>>[-<<<+>>>]<< --- < [-]] >
  <[-]<[-]>> [-<+<+>>] <<[->>+<<]>> < [[->>+>+<<<] >>>[-<<<+>>>]<< --- < [-]] >
  <[-]<[-]>> [-<+<+>>] <<[->>+<<]>> < [[->>+>+<<<] >>>[-<<<+>>>]<< --- < [-]] >
  <[-]<[-]>> [-<+<+>>] <<[->>+<<]>> < [> --- < [-]] >
  <[-]<[-]>> [-<+<+>>] <<[->>+<<]>> < [[->>+>+<<<] >>>[-<<<+>>>]<< --- < [-]] >
]

Memory now: === 0 0 (0) result=233168 0 ===

Print overall result
>.
