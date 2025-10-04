; master library - PC98V
;
; Description:
;	�p���b�g�̏�����
;
; Function/Procedures:
;	void palette_init( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PalettesInit�̓��e��Palettes�ɓ]�ʂ��APaletteTone�� 100�ɐݒ肵�A
;	�A�i���O�p���b�g���[�h�ɐݒ肵�A�\�����܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/17 Initial
;	93/ 6/12 [M0.19] �f�W�^���p���b�g��������

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD
	EXTRN PalettesInit : WORD

	.CODE
	EXTRN	PALETTE_SHOW:CALLMODEL

func PALETTE_INIT
	push	SI
	push	DI
	push	DS
	pop	ES
	mov	DI,offset Palettes
	mov	SI,offset PalettesInit
	mov	CX,48/2
	rep	movsw
	pop	DI
	pop	SI

	mov	AL,0
	out	6ah,AL		; Digital mode
	mov	AL,04h		; initialize digital palette
	out	0aeh,AL
	mov	AL,26h
	out	0ach,AL
	mov	AL,15h
	out	0aah,AL
	mov	AL,37h
	out	0a8h,AL

	mov	AL,1
	out	6ah,AL		; Analog mode
	mov	PaletteTone,100
	call	PALETTE_SHOW
	ret
endfunc

END
