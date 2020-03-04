; master library - VGA
;
; Description:
;	パターンの任意拡大表示
;
; Functions/Procedures:
;	void vga4_super_zoom( int x, int y, int num, int zoom ) ;
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 80186
;	VGA
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
;$Id: superzom.asm 0.02 92/06/10 19:31:57 Kazumi Rel $
;
;	93/ 3/16 Initial: master.lib <- super.lib 0.22b
;	94/ 5/23 Initial: vg4spzom.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

	EXTRN	VGC_SETCOLOR:CALLMODEL
	EXTRN	VGC_BOXFILL:CALLMODEL

func VGA4_SUPER_ZOOM	; vga4_super_zoom() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	num	= (RETSIZE+2)*2
	zoom	= (RETSIZE+1)*2

	push	00f0h ; VGA_PSET
	push	0	; color
	_call	VGC_SETCOLOR		; GCに各種パラメータセット

	mov	SI,[BP+x]
	mov	CS:_X1_,SI
	mov	DI,[BP+y]
	mov	AX,[BP+zoom]
	mov	CS:_NEXT_X_,AX
	mov	CS:_NEXT_Y_,AX
	dec	AX
	mov	CS:_X2_,AX
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	CX,super_patsize[BX]	;pattern size (1-8)
	mov	CS:_X_COUNT_,CH
	mov	ES,super_patdata[BX]
	mov	AX,CX
	xor	AH,AH
	mul	CH
	mov	BP,AX
	mov	CS:_PLANE1_,AX
	add	AX,AX
	mov	CS:_PLANE2_,AX
	add	AX,BP		;AX * 3
	mov	CS:_PLANE3_,AX
	jmp	short next_byte
	EVEN

next_byte:
	push	CX

	mov	BH,ES:[BP]		; B -> BH
	mov	BL,ES:[BP+1111h]	; R -> BL
	org	$-2
_PLANE1_ dw ?
	mov	DH,ES:[BP+1111h]	; G -> DH
	org	$-2
_PLANE2_ dw ?
	mov	DL,ES:[BP+1111h]	; I -> DL
	org	$-2
_PLANE3_ dw ?
	mov	AL,8			;byte(8 bits)

next_bit:
	shl	DL,1
	adc	AH,AH
	shl	DH,1
	adc	AH,AH
	shl	BL,1
	adc	AH,AH
	shl	BH,1
	adc	AH,AH
	and	AH,0fh
	jnz	short no_skip
	add	SI,1111h
	org $-2
_NEXT_X_ dw ?
	dec	AL			; AL=bit count
	jnz	short next_bit
	jmp	short skip
	EVEN
no_skip:
	push	AX
	push	BX
	push	DX
	push	ES

	; vgc_setcolor(VGA_PSET,color); (color=AH)
	mov	DX,VGA_PORT
	mov	AL,VGA_SET_RESET_REG
	out	DX,AX

	JMOV	AX,_X2_

	push	SI	; x1
	push	DI	; y1
	add	SI,AX
	push	SI	; x2
	add	AX,DI
	push	AX	; y2
	_call	VGC_BOXFILL
	inc	SI

	pop	ES
	pop	DX
	pop	BX
	pop	AX
	dec	AL			; AL=bit count
	jnz	short next_bit
skip:
	inc	BP

	pop	CX
	dec	CH			; CH=byte len
	jnz	next_byte
	mov	CH,11h
	org	$-1
_X_COUNT_ db ?
	JMOV	SI,_X1_
	add	DI,1111h
	org	$-2
_NEXT_Y_ dw	?
	dec	CL			; CL=y lines
	jnz	next_byte

	pop	DI
	pop	SI
	pop	BP
	ret	8
endfunc			; }

END
