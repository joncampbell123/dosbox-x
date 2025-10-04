PAGE 98,120
; master library - VGA - SVGA - 16color - boxfill - pset
;
; Description:
;	VGA/SVGA 16�F �����`�h��ׂ� [��8dot�P�ʍ��W, ���Z�Ȃ�]
;
; Function:
;	void vgc_byteboxfill_x_pset( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	x1,y1	���㒸�_	(x���W��0�`79(��640dot��), y���W��0�`)
;	x2,y2	�E�����_
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
;	�E�F������ɂ́Avgc_setcolor()�𗘗p���Ă��������B
;	�@���������Z���[�h�� VGA_PSET�ȊO�ɂ���ƕ`�挋�ʂ͕s��ɂȂ�܂��B
;	�Ex1 <= x2, y1 <= y2 �łȂ���Ε`�悵�܂���B
;	�E�N���b�s���O�͏c�����̂ݍs���Ă��܂��B
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 4/ 8 Initial: vgcbyteb.asm/master.lib 0.23
;	94/ 5/29 BUGFIX�����A��ɑS��ʕ`�悵�Ă�
;	94/ 6/12 Initial: vgcbytbp.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN ClipYT:WORD, ClipYB:WORD
	EXTRN graph_VramSeg:WORD, graph_VramWidth:WORD

	.CODE

MRETURN macro
	pop	BP
	ret	8
	EVEN
	endm

func VGC_BYTEBOXFILL_X_PSET ; vgc_byteboxfill_x_pset() {
	push	BP
	mov	BP,SP
	; ����
	x1	= (RETSIZE+4)*2
	y1	= (RETSIZE+3)*2
	x2	= (RETSIZE+2)*2
	y2	= (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	CX,[BP+y1]
	cmp	CX,AX
	jl	short TOP_OK
	mov	AX,CX
TOP_OK:
	mov	DX,[BP+y2]
	mov	BX,ClipYB
	cmp	DX,BX
	jg	short BOTTOM_OK
	mov	BX,DX
BOTTOM_OK:

	sub	BX,AX		; BX = ylen
	jl	short YCLIPOUT

	push	SI
	push	DI

	mov	DI,[BP+x1]
	mov	SI,[BP+x2]
	sub	SI,DI		; SI = xlen
	jl	short XCLIPOUT
	inc	SI

	mov	BP,graph_VramWidth
	mul	BP
	add	DI,AX		; DI = y1 * VramWidth + x1
	sub	BP,SI		; xadd

	CLD
	mov	ES,graph_VramSeg
	mov	AX,0ffffh
	test	DI,1
	jnz	short ODD_START
	EVEN

EYLOOP:
	mov	CX,SI
	shr	CX,1
	rep	stosw
	adc	CX,CX
	rep	stosb
	add	DI,BP
	dec	BX
	jns	short EYLOOP
XCLIPOUT:
	pop	DI
	pop	SI
YCLIPOUT:
	MRETURN

	EVEN
ODD_START:
OYLOOP:
	mov	CX,SI
	dec	CX
	stosb
	shr	CX,1
	rep	stosw
	adc	CX,CX
	rep	stosb
	add	DI,BP
	dec	BX
	jns	short OYLOOP
	pop	DI
	pop	SI
	MRETURN
endfunc			; }

END
