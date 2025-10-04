; superimpose & master library module
;
; Description:
;	��ʂ������炶�킶��Ɛ���ɂ���
;
; Functions/Procedures:
;	void dac_black_in( int speed ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���s��, PaletteTone �� 100�ɂȂ�܂��B
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
;$Id: blackin.asm 0.01 93/02/19 20:16:20 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	93/ 9/13 [M0.21] palette_show�g�p(�Y��Ă�(^^;)
;	94/ 6/17 Initial: dacblin.asm/master.lib 0.23
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes : BYTE
	EXTRN PaletteTone : WORD

	.CODE
	EXTRN	VGA_VSYNC_WAIT:CALLMODEL
	EXTRN	DAC_SHOW:CALLMODEL

func DAC_BLACK_IN	; dac_black_in() {
	mov	BX,SP
	push	SI
	push	DI
	; ����
	speed	= (RETSIZE+0)*2
	mov	SI,SS:[BX+speed]

	mov	PaletteTone,0
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
	cmp	PaletteTone,100
	jl	short MLOOP

	mov	PaletteTone,100
	call	DAC_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

END
