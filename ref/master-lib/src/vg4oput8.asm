; superimpose & master library module
;
; Description:
;	VGA 16color, 透明処理を行わないパターン表示[横8dot単位]
;
; Functions/Procedures:
;	void vga4_over_put_8( int x, int y, int num ) ;
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
;	VGA
;
; Requiring Resources:
;	CPU: 186
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
;	93/12/ 8 Initial: vg4oput8.asm/master.lib 0.22
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD

COPY macro
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
endm

	.CODE
func VGA4_OVER_PUT_8	; vga4_over_put_8() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	CLD
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	AX,[BP+y]
	mov	BP,[BP+x]

	mov	ES,graph_VramSeg
	mov	DI,graph_VramWidth
	mul	DI
	shr	BP,3		;BP=x/8
	add	BP,AX		;GVRAM offset address

	mov	CX,super_patsize[BX]		;pattern size (1-8)
	mov	DS,super_patdata[BX]		;BX+2 -> BX

	mov	AL,CH		; skip mask pattern
	mul	CL
	mov	SI,AX

	mov	BX,DI
	sub	BL,CH

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
;	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
;	out	DX,AX
;	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
;	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
	out	DX,AX

	; B
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	out	DX,AX
	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
LOOP1:
	mov	CL,AH
	COPY
	add	DI,BX
	dec	DH
	jnz	short LOOP1
	mov	CX,AX

	; R
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	out	DX,AX
	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
LOOP2:
	mov	CL,AH
	COPY
	add	DI,BX
	dec	DH
	jnz	short LOOP2
	mov	CX,AX

	; G
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	out	DX,AX
	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
LOOP3:
	mov	CL,AH
	COPY
	add	DI,BX
	dec	DH
	jnz	short LOOP3
	mov	CX,AX

	; I
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	out	DX,AX
	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
LOOP4:
	mov	CL,AH
	COPY
	add	DI,BX
	dec	DH
	jnz	short LOOP4

;	mov	DX,SEQ_PORT
;	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
;	out	DX,AX

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

END
