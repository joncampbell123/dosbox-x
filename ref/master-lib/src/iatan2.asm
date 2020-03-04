; Description:
;	atan2ÇÃêÆêî(256ìxån)î≈
;
; Function
;	int pascal iatan2( int y, int x ) ;
;
; Author:
;	óˆíÀè∫ïF
;
; History:
;	92/ 7/29 Initial
;	93/ 5/29 [M0.18] .CONST->.DATA
;
	.186
	.MODEL SMALL
	.DATA

	EXTRN	AtanTable8:BYTE

	.CODE
	include func.inc

MRETURN macro
	pop	BP
	ret	4
	EVEN
	endm

; int iatan2( int y, int x ) ;
func IATAN2
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
	mov	BX,offset AtanTable8
	xlatb
	jmp	short L200
	EVEN

L100:	mov	AL,32
	jmp	short L200
	EVEN

L150:	mov	AX,DX
	xor	DH,DH
	mov	DL,AH
	mov	AH,AL
	mov	AL,DH
	div	BX
	mov	BX,offset AtanTable8
	xlatb
	neg	AL
	add	AL,64
L200:
	xor	AH,AH

	or	BP,BP
	jge	short L300
	neg	AX
	add	AX,128
L300:
	or	CX,CX
	jge	short L400
	neg	AL		; ax = 256 - axÇæÇØÇ«Åcè»ó™
L400:
	xor	AH,AH
RETURN:
	MRETURN
endfunc
END
