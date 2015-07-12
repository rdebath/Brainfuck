	EXEOBJ
	OUTPUT	bfc

OBJSIZE	EQU	65536
CODSIZE	EQU	32000

read	EQU	-$2a
write	EQU	-$30
input	EQU	-$36
output	EQU	-$3c
closlib EQU	-$19e

; d0
; d1   len
; d2
; d3   1
; d4   byte
; d5   offset
; d6   
; d7

; a0
; a1
; a2   stack
; a3   cdptr
; a4   4
; a5   code
; a6


initcode:
	dc.l	$3f3,0,1,0,0,OBJSIZE/4-9,$3e9,OBJSIZE/4-9

	lea	(CODSIZE,pc),a5
	moveq	#4,d0
	move.l	d0,a4
	move.l	(a4),a6
	jsr	-810(a6)
	move.l	d0,a6
	moveq	#1,d3
initcode2:



clrmem
	move.l	a5,a2
	moveq	#-1,d5
..	clr.b	(a2)+
	dbra	d5,..
	move.w	#$3f2,-(a2)
	move.l	a5,a2



	move.b	d3,(a5)
mainloop
	move.b	(a5),d4
	lea	(code,pc),a3

..	move.b	(a3)+,d5
	move.b	(a3)+,d1
	cmp.b	(a3)+,d4
	blo.b	..
	bne.b	advance
	add.l	d5,a3

copy	move.b	(a3)+,(a5)+
	subq.b	#1,d1
	bne.b	copy

	addq.b	#8,d5
	bcc.s	noloop
	move.l	a5,-(a2)
noloop

	addq.b	#3,d5
	bne.s	noendl
	move.l	(a2)+,a0
	move.l	a0,d0
	sub.l	a5,d0
	move.w	d0,(a5)+
	neg.w	d0
	subq.w	#4,d0
	move.w	d0,-(a0)
noendl


advance
	clr.b	(a5)
readbyte
	jsr	input(a6)
	move.l	d0,d1
 	move.l	a5,d2
	jsr	read(a6)
readbyte2

	tst.b	d4
	bne.s	mainloop

	move.l	a2,a5
	swap	d3
writebyte
	jsr	output(a6)
	move.l	d0,d1
	move.l	a5,d2
	jsr	write(a6)
writebyte2

cleanup
	move.l	a6,a1
	move.l	(a4),a6
	jsr	closlib(a6)
	moveq	#0,d0
	rts
cleanup2

rightcode:
	addq.l	#1,a5
leftcode:
	subq.l	#1,a5

endwcode
addcode:
	addq.b	#1,(a5)
subcode:
	subq.b	#1,(a5)
	dc.w	$6600
endwcode2

whilecode:
	dc.w	$6000
whilecode2

code:
endw:	dc.b	endwcode-endw-3,6,']'
while:	dc.b	whilecode-while-3,4,'['
right:	dc.b	rightcode-right-3,2,'>'
left:	dc.b	leftcode-left-3,2,'<'

out:	dc.b	writebyte-out-3,writebyte2-writebyte,'.'
sub:	dc.b	subcode-sub-3,2,'-'
in:	dc.b	readbyte-in-3,readbyte2-readbyte,','
add:	dc.b	addcode-add-3,2,'+'

beg:	dc.b	(initcode-beg-3)&$FF,initcode2-initcode,1
end:	dc.b	cleanup-end-3,cleanup2-cleanup,0

	dx.b	OBJSIZE+CODSIZE

	END
