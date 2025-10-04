; superimpose & master library module
;
; Description:
;	画面の上下をつないだ透明処理を行わないパターン表示[横8dot単位]
;
; Functions/Procedures:
;	void over_roll_put_8( int x, int y, int num ) ;
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
;	400line固定です。
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
;$Id: overoll8.asm 0.02 92/05/29 20:06:53 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE
func OVER_ROLL_PUT_8
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	AX,[BP+y]
	mov	DI,AX		;-+
	shl	DI,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	shr	CX,3		;CX=x/8
	add	DI,CX		;GVRAM offset address
	mov	CS:_DI_,DI

	mov	BX,[BP+num]	;integer size & near pointer
	shl	BX,1
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	DS,super_patdata[BX]	;BX+2 -> BX

	mov	BX,DX
	xor	BH,BH
	add	BX,AX			;y + size *8
	sub	BX,400
	jg	short SKIP
	xor	BX,BX
SKIP:

	; SKIP mask pattern
	mov	AL,DH
	mul	DL
	mov	SI,AX

	mov	AX,80
	sub	AL,DH

	mov	CL,DH
	xor	CH,CH
	mov	BP,CX
	sub	DL,BL

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
; 表示サブルーチン
; in:
;	BP = CX = 長さ(unit: byte)
DISP	proc	near
	mov	ES,CX

	mov	BH,BL
	mov	DH,DL

	JMOV	DI,_DI_
	test	DI,1
	jnz	short ODD_ADDRESS

	; 開始アドレスが偶数
	shr	BP,1
	jb	short EAOS
	EVEN

	; 開始アドレスが偶数で長さが奇数
EAES:		; ←←←
	mov	CX,BP
	rep	movsw
	add	DI,AX
	dec	DH
	jnz	short EAES

	or	BH,BH
	jnz	short ROLL1
	shl	BP,1
	ret
	EVEN
ROLL1:
	sub	DI,7d00h
	mov	DH,BH
	xor	BH,BH
	jmp	short EAES
	EVEN

	; 開始アドレスが偶数で長さが偶数
ROLL2:
	sub	DI,7d00h
	mov	DH,BH
	xor	BH,BH
	EVEN
EAOS:		; ←←←
	mov	CX,BP
	rep	movsw
	movsb
	add	DI,AX
	dec	DH
	jnz	short EAOS
	or	BH,BH
	jnz	short ROLL2
	stc
	rcl	BP,1
	ret

	; 開始アドレスが奇数
	EVEN
ODD_ADDRESS:
	shr	BP,1
	jb	short OAOS

	; 開始アドレスが奇数で長さが偶数
	EVEN
OAES:		; ←←←
	mov	CX,BP
	dec	CX
	movsb
	rep	movsw
	movsb
	add	DI,AX
	dec	DH
	jnz	short OAES
	or	BH,BH
	jnz	short ROLL3
	shl	BP,1
	ret
	EVEN
ROLL3:
	sub	DI,7d00h
	mov	DH,BH
	xor	BH,BH
	jmp	short OAES


	; 開始アドレスが奇数で長さが奇数
	EVEN
ROLL4:
	sub	DI,7d00h
	mov	DH,BH
	xor	BH,BH
	EVEN
OAOS:		; ←←←
	mov	CX,BP
	movsb
	rep	movsw
	add	DI,AX
	dec	DH
	jnz	short OAOS
	or	BH,BH
	jnz	short ROLL4
	stc
	rcl	BP,1
	ret
DISP	endp

END
