; clip.inc
; 92/6/19
; A.Koizuka

	; アウトコード
LEFT_OUT	EQU	0001b
RIGHT_OUT	EQU	0010b
YOKO_OUT	EQU	0011b
BOTTOM_OUT	EQU	0100b
TOP_OUT		EQU	1000b
TATE_OUT	EQU	1100b

.xlist

; IN: x,y
; OUT: ans
; BREAK: ans, flag
GETOUTCODE macro ans,x,y,xl,xr,yh
	local OUT1,OUTEND
;	mov	ans,LEFT_OUT or BOTTOM_OUT
	cmp	xl,x
	jg	short OUT1
	xor	ans,YOKO_OUT
	cmp	x,xr
	jg	short OUT1
	xor	ans,RIGHT_OUT
OUT1:
	cmp	y,yh
	jg	short OUTEND
	xor	ans,TATE_OUT
	or	y,y
	js	short OUTEND
	xor	ans,TOP_OUT
OUTEND:
	ENDM

ifndef clipasm
	EXTRN cutline:NEAR
	; cutline:
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !o2:o1! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+
	; カットカット～～～(x1,y1)を切断により変更する
	; in: BH:o2, BL:o1
	; out: BH:o2, BL:o1, o1がzeroならば zflag=1
endif
.list
