; Description:
;	atan2ÇÃêÆêî(360ìxån)î≈
;
; Function
;	int pascal iatan2deg( int y, int x ) ;
;
; Author:
;	óˆíÀè∫ïF
;
; History:
;	92/ 7/29 Initial: iatan2.asm
;	92/ 8/15 Initial: iatandeg.asm/master.lib 0.21
;
	.186
	.MODEL SMALL
	.DATA

	EXTRN	AtanTableDeg:BYTE

	.CODE
	include func.inc

MRETURN macro
	pop	BP
	ret	4
	EVEN
	endm

; int iatan2deg( int y, int x ) ;
func IATAN2DEG
	push	BP
	mov	BP,SP

	; à¯êî
	y = (RETSIZE+2)*2
	x = (RETSIZE+1)*2

	mov	CX,[BP+y]
	mov	BP,[BP+x]

	mov	AX,CX
	or	AX,BP
	jz	short RETURN

	mov	AX,CX
	cwd
	xor	AX,DX
	sub	AX,DX
	mov	BX,AX			; BX = abs(y)

	mov	AX,BP
	cwd
	xor	AX,DX
	sub	AX,DX
	mov	DX,AX			; DX = abs(x)

	cmp	DX,BX			; abs(x) <= abs(y) -> jump
	je	short L100
	jl	short L150

	mov	AX,BX
	mov	BX,DX
	xor	DH,DH
	mov	DL,AH
	mov	AH,AL
	mov	AL,DH
	div	BX
	mov	BX,offset AtanTableDeg
	xlatb
	jmp	short L200
	EVEN

L100:	mov	AL,45
	jmp	short L200
	EVEN

L150:	mov	AX,DX
	xor	DH,DH
	mov	DL,AH
	mov	AH,AL
	mov	AL,DH
	div	BX
	mov	BX,offset AtanTableDeg
	xlatb
	neg	AL
	add	AL,90
L200:
	xor	AH,AH

	or	BP,BP
	jge	short L300
	neg	AX
	add	AX,180
L300:
	or	CX,CX
	jge	short L400
	sub	AX,360
	neg	AX		; ax = 360 - ax
L400:
RETURN:
	MRETURN
endfunc
END
