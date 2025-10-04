; master library - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂ̏I������(400line,����,�\��OFF)
;
; Function/Procedures:
;	void graph_end(void) ;
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
;	92/12/28 ��\�����ŏ��ɂ����Ă���

	.MODEL SMALL
	include func.inc

	.CODE

	EXTRN GRAPH_400LINE:CALLMODEL
	EXTRN GRAPH_CLEAR:CALLMODEL
	EXTRN GRAPH_HIDE:CALLMODEL

func GRAPH_END
	call	GRAPH_HIDE
	call	GRAPH_400LINE
	call	GRAPH_CLEAR
	ret
endfunc

END
