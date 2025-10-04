; master library - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂ̏����ݒ肻��2
;	(�A�i���O16�F,400line,��������,�풓�p���b�g�ǂ݂���,�\��)
;
; Function/Procedures:
;	void graph_enter(void) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 3/10 Initial
;	93/ 6/12 [M0.19] �\��ON�ɂ��Ă���400line�ɂ���悤�ɂ���
;	93/12/27 [M0.22] �풓�p���b�g���Ȃ���΃A�i���O�ɂ��邾���ɂ���
;	94/ 1/22 [M0.22] �풓�p���b�g���Ȃ��ăn�[�h�p���b�g���ǂ߂�Ȃ�ǂ�
;	95/ 3/20 [M0.22k] ���S�̂��ߊJ�n����grcg_off����ꂽ

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes:WORD
	EXTRN PalettesInit:WORD
	EXTRN PaletteTone:WORD

	.CODE

	EXTRN RESPAL_EXIST:CALLMODEL
	EXTRN RESPAL_GET_PALETTES:CALLMODEL
	EXTRN PALETTE_SHOW:CALLMODEL
	EXTRN GRAPH_400LINE:CALLMODEL
	EXTRN GRAPH_SHOW:CALLMODEL

func GRAPH_ENTER	; graph_enter() {
	mov	AL,41h		; �e�L�X�g�^�O���t��ʃh�b�g�Y���␳
	out	6Ah,AL		; mode flipflop2
	mov	AL,1
	out	6ah,AL		; Analog mode

	xor	AL,AL		; GRCG OFF
	out	7ch,AL

	call	RESPAL_EXIST	; includes 'CLD'
	test	AX,AX
	jz	short NO_RESPAL
	call	RESPAL_GET_PALETTES
	call	PALETTE_SHOW
	jmp	short CONT

NO_RESPAL:
	mov	PaletteTone,100
	push	SI
	push	DI
	push	DS
	pop	ES
	mov	DI,offset Palettes

	out	0a8h,AL	; palette number 0
	in	AL,0ach
	cmp	AL,0fh
	ja	short DEFAULT_SET

	; �p���b�g���ǂ߂���Ȃ�
	mov	CX,4	; CH=0
READLOOP:
	mov	AL,CH
	out	0a8h,AL	; palette number
	in	AL,0ach	; r
	mov	AH,AL
	shl	AL,CL
	or	AL,AH
	stosb
	in	AL,0aeh	; b
	mov	AH,AL
	in	AL,0aah	; g
	mov	BX,AX
	shl	AX,CL
	or	AX,BX
	stosw
	inc	CH
	cmp	CH,16
	jl	short READLOOP
	jmp	short SETCONT

DEFAULT_SET:
	mov	SI,offset PalettesInit
	mov	CX,16*3/2
	rep	movsw
SETCONT:
	pop	DI
	pop	SI

CONT:
	call	GRAPH_SHOW
	call	GRAPH_400LINE
	ret
endfunc			; }

END
