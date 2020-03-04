; superimpose & master library module
;
; Description:
;	パターン表示 [横8dot単位]
;
; Functions/Procedures:
;	void super_put_8( int x, int y, int num ) ;
;
; Parameters:
;	x,y	座標
;	num	パターン番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	
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
;$Id: superpt8.asm 0.07 92/05/29 20:37:21 Kazumi Rel $
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

func SUPER_PUT_8
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
	mov	BX,[BP+num]

	mov	BP,AX		;-+
	shl	AX,2		; |
	add	BP,AX		; |DI=y*80
	shl	BP,4		;-+
	shr	CX,3		;CX=x/8
	add	BP,CX		;GVRAM offset address
	shl	BX,1		;integer size & near pointer
	mov	AX,super_patsize[BX]	;pattern size (1-8)
	xor	SI,SI
	mov	DS,super_patdata[BX]	;BX+2 -> BX

	mov	BL,AH
	xor	BH,BH
	mov	DX,80
	sub	DL,AH
	mov	AH,AL

	mov	CX,0a800h
	mov	ES,CX

	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,0
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP		;cls

	mov	AL,11001110b
	out	7ch,AL		;RMW mode
	mov	AL,0ffh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	call	DISP
	mov	AL,11001101b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	AL,11001011b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	AL,11000111b
	out	7ch,AL		;RMW mode
	call	DISP

	xor	AL,AL
	out	7ch,AL		;grcg off

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc

;
; 表示サブルーチン
;
DISP	proc	near
	mov	DI,BP

	mov	AL,AH
	test	DI,1
	jnz	short ODD_ADDRESS

	shr	BX,1
	jb	short EAOS

	EVEN
EAES:
	mov	CX,BX
	rep	movsw
	add	DI,DX
	dec	AL
	jnz	short EAES
	shl	BX,1
	ret

	EVEN
EAOS:
	mov	CX,BX
	rep	movsw
	movsb
	add	DI,DX
	dec	AL
	jnz	short EAOS
	stc
	rcl	BX,1
	ret
	EVEN

ODD_ADDRESS:
	shr	BX,1
	jb	short OAOS
	EVEN
OAES:
	mov	CX,BX
	dec	CX
	movsb
	rep	movsw
	movsb
	add	DI,DX
	dec	AL
	jnz	short OAES
	shl	BX,1
	ret

	EVEN
OAOS:
	mov	CX,BX
	movsb
	rep	movsw
	add	DI,DX
	dec	AL
	jnz	short OAOS
	stc
	rcl	BX,1
	ret
DISP	endp

END
