; superimpose & master library module
;
; Description:
;	大きさを縦横2倍に拡大してパターンを表示する
;
; Functions/Procedures:
;	void vga4_super_large_put( int x, int y, int num ) ;
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
;	VGA 16Color
;
; Requiring Resources:
;	CPU: 
;
; Notes:
;	クリッピングしてません
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
;$Id: largeput.asm 0.04 92/05/29 20:06:02 Kazumi Rel $
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	94/ 6/ 8 Initial: vg4lgput.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD

	.CODE
	EXTRN	LARGE_BYTE:BYTE		; code segment!

func VGA4_SUPER_LARGE_PUT	; vga4_super_large_put() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg

	mov	CX,[BP+x]
	mov	AX,graph_VramWidth
	mul	word ptr [BP+y]
	mov	DI,AX
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	CS:_DI_,DI

	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	xor	SI,SI
	mov	AX,graph_VramWidth
	mov	CS:_XSIZE_,DH
	mov	CS:_YSIZE_,DL
	sub	AL,DH
	shl	AX,1
	mov	CS:add_di,AX
	mov	DS,super_patdata[BX]

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_SET_RESET_REG or (0 shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	call	DISP8		;originally cls_loop

	mov	DX,VGA_PORT
	mov	AX,VGA_SET_RESET_REG or (0fh shl 8)
	out	DX,AX

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	DISP8
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	call	DISP8
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	DISP8
	mov	AL,11000111b
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	DISP8

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

	; IN:
	;	AH
	;	DH
DISP8	proc near
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	CH,11h		;dummy
	org	$-1
_YSIZE_	db	?
	JMOV	DI,_DI_
	EVEN

PUT_LOOP0:
	mov	DL,0		; 2bytes=even
	org	$-1
_XSIZE_	db	?

PUT_LOOP:
	lodsb
	mov	BP,AX
	and	AX,00f0h
	shr	AX,4
	mov	BX,AX
	mov	AL,CS:LARGE_BYTE[BX]
	xor	AH,AH
	ror	AX,CL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	test	ES:[DI+80],AL
	mov	ES:[DI+80],AL
	test	ES:[DI+81],AH
	mov	ES:[DI+81],AH
	inc	DI
	and	BP,000fh
	mov	AL,CS:LARGE_BYTE[BP]
	xor	AH,AH
	ror	AX,CL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	test	ES:[DI+80],AL
	mov	ES:[DI+80],AL
	test	ES:[DI+81],AH
	mov	ES:[DI+81],AH
	inc	DI
	dec	DL
	jnz	short PUT_LOOP

	add	DI,1111h	;dummy
	org	$-2
add_di	dw	?
	dec	CH
	jnz	short PUT_LOOP0

	ret
DISP8	endp

END
