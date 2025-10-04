; master library module - VGA
;
; Description:
;	�O���t�B�b�N��ʂւ�BFNT�����`��
;
; Functions/Procedures:
;	void vgc_bfnt_putc( int x, int y, int c ) ;
;
; Parameters:
;	x,y	�`��J�n������W
;	c	���p����
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�N���b�s���O���Ă܂���
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	$Id: ankputs.asm 0.01 92/06/06 14:45:43 Kazumi Rel $
;
;	93/ 2/25 Initial: master.lib <- super.lib 0.22b
;	93/ 7/23 [M0.20] far������ɂ���ƑΉ�(^^;
;	                 ����graph_VramSeg�g���悤�ɂ���
;	94/ 1/24 Initial: grpbputs.asm/master.lib 0.22
;	94/ 4/14 Initial: grpbputc.asm/master.lib 0.23
;	94/ 4/14 Initial: vgcbputc.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

func VGC_BFNT_PUTC	; vgc_bfnt_putc() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; ����
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	c	= (RETSIZE+1)*2

	CLD

	mov	CX,[BP+x]
	mov	DI,CX
	and	CX,7
	shr	DI,3
	mov	AX,[BP+y]
	mul	graph_VramWidth
	add	DI,AX
	mov	ES,graph_VramSeg

	mov	AL,[BP+c]
	mul	byte ptr font_AnkPara
	add	AX,font_AnkSeg

	mov	BX,font_AnkSize		; BL=ylen   BH=xlen
	mov	BP,graph_VramWidth
	mov	DH,0
	mov	DL,BH
	sub	BP,DX			; xadd

	EVEN
	mov	DS,AX		; pattern seg
	xor	SI,SI
DRAWY:
	mov	CH,BH
	mov	DL,0
DRAWX:
	mov	AH,0
	lodsb
	ror	AX,CL
	or	AL,DL
	mov	DL,AH
	test	ES:[DI],AL	; fill latch
	stosb
	dec	CH
	jnz	short DRAWX
	test	ES:[DI],AH	; fill latch
	mov	ES:[DI],AH

	add	DI,BP
	dec	BL
	jnz	short DRAWY

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	3*2
endfunc			; }

END
