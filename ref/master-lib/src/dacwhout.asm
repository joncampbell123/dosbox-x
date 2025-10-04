; superimpose & master library module
;
; Description:
;	��ʂ����킶��Ɛ^�����ɂ���
;
; Functions/Procedures:
;	dac_white_out
;
; Parameters:
;	speed	�x������(vsync�P��) 0=�x���Ȃ�
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
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
;$Id: whiteout.asm 0.01 93/02/19 20:13:01 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 6/25 [M0.19] palette_show�g�p
;	94/ 6/17 Initial: dacwhout.asm/master.lib 0.23
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes:BYTE
	EXTRN PaletteTone:WORD

	.CODE
	EXTRN	VGA_VSYNC_WAIT:CALLMODEL
	EXTRN	DAC_SHOW:CALLMODEL

func DAC_WHITE_OUT	; dac_white_out {
	mov	BX,SP
	push	SI
	push	DI
	; ����
	speed	= (RETSIZE+0)*2
	mov	SI,SS:[BX+speed]

	mov	PaletteTone,100
	call	VGA_VSYNC_WAIT	; �\���^�C�~���O���킹
MLOOP:
	call	DAC_SHOW
	mov	DI,SI
	cmp	DI,0
	jle	short SKIP
VWAIT:
	call	VGA_VSYNC_WAIT
	dec	DI
	jnz	short VWAIT
SKIP:

	add	PaletteTone,6
	cmp	PaletteTone,200
	jl	short MLOOP

	mov	PaletteTone,200
	call	DAC_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

END
