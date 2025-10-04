PAGE 98,120
; master library - VGA - SVGA - 16color - boxfill
;
; Description:
;	VGA/SVGA 16�F �����`�h��ׂ� [��8dot�P�ʍ��W]
;
; Function:
;	void vgc_byteboxfill_x( int x1, int y1, int x2, int y2 ) ;
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

func VGC_BYTEBOXFILL_X ; vgc_byteboxfill_x() {
	push	BP
	mov	BP,SP
	; ����
	x1	= (RETSIZE+4)*2
	y1	= (RETSIZE+3)*2
	x2	= (RETSIZE+2)*2
	y2	= (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	BX,[BP+y1]
	cmp	BX,AX
	jl	short TOP_OK
	mov	AX,BX
TOP_OK:
	mov	DX,[BP+y2]
	mov	CX,ClipYB
	cmp	DX,CX
	jg	short BOTTOM_OK
	mov	CX,DX
BOTTOM_OK:

	sub	CX,AX		; CX = ylen
	jl	short YCLIPOUT

	push	SI
	push	DI

	mov	DI,[BP+x1]
	mov	SI,[BP+x2]
	sub	SI,DI		; SI = xlen
	jl	short XCLIPOUT

	mov	BP,graph_VramWidth
	mul	BP
	add	DI,AX		; DI = ylen * VramWidth + x1

	mov	ES,graph_VramSeg
	inc	CX
	mov	AL,0ffh
	EVEN
YLOOP:
	mov	BX,SI
XLOOP:
	or	ES:[DI+BX],AL
	dec	BX
	jns	short XLOOP
	add	DI,BP
	loop	short YLOOP
XCLIPOUT:
	pop	DI
	pop	SI
YCLIPOUT:
	MRETURN
endfunc			; }

END
