; master library - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂ̏����ݒ�(�A�i���O16�F,400line,����,�\��)
;
; Function/Procedures:
;	void graph_start(void) ;
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
;	92/11/21 Initial
;	93/ 5/17 [M0.17] ��ɉ�ʂ��Â�����悤�ɂ���
;	93/ 6/12 [M0.19] �Â�������ŏ��ɕ\��ON�ɂ���悤�ɂ���
;	93/10/15 [M0.21] �\����,�A�N�Z�X�y�[�W�� 0 �ɂ���悤�ɂ��� (from iR)

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN PaletteTone : WORD

	.CODE

	EXTRN PALETTE_INIT:CALLMODEL
	EXTRN GRAPH_400LINE:CALLMODEL
	EXTRN GRAPH_CLEAR:CALLMODEL
	EXTRN GRAPH_SHOW:CALLMODEL
	EXTRN PALETTE_SHOW:CALLMODEL

func GRAPH_START
	mov	AL,41h		; �e�L�X�g�^�O���t��ʃh�b�g�Y���␳
	out	6Ah,AL		; mode flipflop2

	mov	PaletteTone,0	; ��ɈÂ�����
	call	PALETTE_SHOW

	mov	AL,0
	out	0a4h,AL		; show page
	out	0a6h,AL		; access page

	call	GRAPH_SHOW
	call	GRAPH_400LINE
	call	GRAPH_CLEAR
	call	PALETTE_INIT
	ret
endfunc

END
