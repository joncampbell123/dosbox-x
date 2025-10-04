; master library module - DAC
;
; Description:
;	��ʂ����킶��Ɛ^�����ɂ���
;
; Functions/Procedures:
;	void dac_black_out( int speed ) ;
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
;	���ˏ��F
;
; Revision History:
;	94/ 5/23 Initial: master.lib 0.23

	.MODEL SMALL
	include func.inc

	.DATA

;	EXTRN	Palettes:BYTE
	EXTRN	PaletteTone:WORD

	.CODE
	EXTRN	VGA_VSYNC_WAIT:CALLMODEL
	EXTRN	DAC_SHOW:CALLMODEL

func DAC_BLACK_OUT	; dac_black_out() {
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

	sub	PaletteTone,6
	jg	short MLOOP

	mov	PaletteTone,0
	call	DAC_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

END
