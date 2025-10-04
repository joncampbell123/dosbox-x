; superimpose & master library module
;
; Description:
;	VGA 16color, �����������s��Ȃ��p�^�[���\��[��8dot�P��]
;
; Functions/Procedures:
;	void vga4_slice_put( int x, int y, int num, int line ) ;
;
; Parameters:
;	x,y	����[�̍��W
;	num	�p�^�[���ԍ�
;	line	�`�悷��p�^�[����y�ʒu
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
;	�����W��8�̔{���ɐ؂�̂Ă���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: overput8.asm 0.04 92/05/29 20:08:09 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]
;	93/12/ 8 Initial: vg4oput8.asm/master.lib 0.22
;	95/ 6/ 7 Initial: vg4slput.asm / master.lib 0.23
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
func VGA4_SLICE_PUT	; vga4_slice_put() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	num	= (RETSIZE+2)*2
	line	= (RETSIZE+1)*2

	CLD
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	AX,[BP+y]

	mov	ES,graph_VramSeg
	mov	DI,graph_VramWidth
	mul	DI

	mov	DI,[BP+x]
	shr	DI,3		;BP=x/8
	add	DI,AX		;GVRAM offset address

	mov	CX,super_patsize[BX]
	mov	DS,super_patdata[BX]

	mov	AL,CH		; skip mask pattern
	mul	CL
	mov	SI,AX
	mov	BX,AX
	mov	AL,[BP+line]
	mul	CL
	add	SI,AX		; offset read address
	mov	CL,CH
	mov	CH,0
	sub	BX,CX

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
	out	DX,AX

	; B
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	out	DX,AX
	mov	AX,CX
	COPY
	sub	DI,AX
	add	SI,BX
	mov	CX,AX

	; R
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	out	DX,AX
	mov	AX,CX
	COPY
	sub	DI,AX
	add	SI,BX
	mov	CX,AX

	; G
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	out	DX,AX
	mov	AX,CX
	COPY
	sub	DI,AX
	add	SI,BX
	mov	CX,AX

	; I
	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	out	DX,AX
	COPY

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	8
endfunc			; }

END
