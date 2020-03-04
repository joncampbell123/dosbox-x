; superimpose & master library module
;
; Description:
;	透明処理を行わないパターン表示[横8dot単位]
;
; Functions/Procedures:
;	void over_put_8( int x, int y, int num ) ;
;
; Parameters:
;	x,y	左上端の座標
;	num	パターン番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	横座標は8の倍数に切り捨てられる。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: overput8.asm 0.04 92/05/29 20:08:09 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE
func OVER_PUT_8
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	CX,[BP+x]
	mov	BP,[BP+y]

	mov	AX,BP		;-+
	shl	AX,2		; |
	add	BP,AX		; |DI=y*80
	shl	BP,4		;-+
	shr	CX,3		;CX=x/8
	add	BP,CX		;GVRAM offset address

	mov	DX,super_patsize[BX]		;pattern size (1-8)
	mov	DS,super_patdata[BX]		;BX+2 -> BX

	mov	AL,DH		; skip mask pattern
	mul	DL
	mov	SI,AX

	mov	BX,80
	sub	BL,DH
	mov	AL,DH
	xor	AH,AH
	mov	CX,0a800h
	call	DISP
	mov	CX,0b000h
	call	DISP
	mov	CX,0b800h
	call	DISP
	mov	CX,0e000h
	call	DISP

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc

;
; 表示
;
; IN:
;	BP
;	CX
;	DL
;	AX
;	DS:SI
;	BX
DISP	proc	near
	mov	ES,CX

	mov	DH,DL
	mov	DI,BP
	test	DI,1
	jnz	short ODD_ADDRESS
	shr	AX,1
	jb	short EAOS

	EVEN
EAES:
	mov	CX,AX
	rep	movsw
	add	DI,BX
	dec	DH
	jnz	short EAES
	shl	AX,1
	ret

	EVEN
EAOS:
	mov	CX,AX
	rep	movsw
	movsb
	lea	DI,[DI+BX]
	dec	DH
	jnz	short EAOS
	rcl	AX,1
	ret

	EVEN
ODD_ADDRESS:
	shr	AX,1
	jb	short OAOS

	EVEN
OAES:
	dec	AX
OAES_L:
	mov	CX,AX
	movsb
	rep	movsw
	movsb
	add	DI,BX
	dec	DH
	jnz	short OAES_L
	inc	AX
	shl	AX,1
	ret

	EVEN
OAOS:
	mov	CX,AX
	movsb
	rep	movsw
	lea	DI,[DI+BX]
	dec	DH
	jnz	short OAOS
	rcl	AX,1
	ret
DISP	endp

END
