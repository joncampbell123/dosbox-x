; master library module
;
; Description:
;	�O���t�B�b�N��ʂւ�BFNT�����`��
;
; Functions/Procedures:
;	void graph_bfnt_putc( int x, int y, int step, int c, int color ) ;
;
; Parameters:
;	x,y	�`��J�n������W
;	step	�������Ƃɐi�߂�h�b�g��(0=�i�߂Ȃ�)
;	c	���p����
;	color	�F(0�`15)
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
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
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE
SCREEN_XBYTE equ 80

func GRAPH_BFNT_PUTC	; graph_bfnt_putc() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; ����
	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	c	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	CLD
	mov	CX,[BP+color]

	; �F�̐ݒ�
	mov	AL,0c0h		;RMW mode
	pushf
	CLI
	out	7ch,AL
	popf
	REPT 4
	shr	CX,1
	sbb	AL,AL
	out	7eh,AL
	ENDM

	mov	CX,[BP+x]
	mov	DI,CX
	and	CX,7
	shr	DI,3
	mov	AX,SCREEN_XBYTE
	mul	word ptr [BP+y]
	add	DI,AX
	mov	ES,graph_VramSeg

	mov	AL,[BP+c]
	mul	byte ptr font_AnkPara
	add	AX,font_AnkSeg
	xor	SI,SI

	mov	BX,font_AnkSize		; BL=ylen   BH=xlen
	mov	BP,SCREEN_XBYTE
	mov	DH,0
	mov	DL,BH
	sub	BP,DX			; xadd = SCREEN_XBYTE-xlen

	EVEN
	mov	DS,AX		; ank seg
DRAWY:
	mov	CH,BH
	mov	DL,0
DRAWX:
	mov	AH,0
	lodsb
	ror	AX,CL
	or	AL,DL
	mov	DL,AH
	stosb
	dec	CH
	jnz	short DRAWX
	mov	ES:[DI],AH

	add	DI,BP
	dec	BL
	jnz	short DRAWY

	; GRCG��؂�
	mov	AL,0
	out	7ch,AL

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	4*2
endfunc			; }

END
