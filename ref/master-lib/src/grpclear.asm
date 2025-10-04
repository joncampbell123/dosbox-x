; master library - PC98V - GRCG
;
; Description:
;	��ʂ̏���
;
; Function/Procedures:
;	void graph_clear( void ) ;
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
;	GRAPHICS ACCERALATOR: GRCG
;
; Notes:
;	���݂̃A�N�Z�X��ʂ��������܂��B
;	GRCG�̃^�C�����W�X�^�� all zero�ɁA���[�h�� off�ɂ��܂��B
;	���荞�݂������Ă��܂��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial
;	92/11/20 bugfix

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg : WORD
	EXTRN graph_VramWords : WORD

	.CODE

func	GRAPH_CLEAR
	mov	AL,080h	; GRCG = TDW black
	pushf
	CLI
	out	7ch,AL
	popf
	xor	AX,AX
	mov	DX,7eh
	out     DX,AL
	out     DX,AL
	out     DX,AL
	out     DX,AL

	mov	BX,DI		; save DI
	xor	DI,DI		; �����J�n�̾�ı��ڽ
	mov	CX,graph_VramWords
	mov	ES,graph_VramSeg
	rep	stosw		; �h��Ԃ�
	mov	DI,BX		; restore DI

	out	7ch,AL		; GRCG OFF
	ret
endfunc

END
