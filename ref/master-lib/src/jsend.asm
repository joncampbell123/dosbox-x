; master library - 
;
; Description:
;	�W���C�X�e�B�b�N����̏I��
;
; Function/Procedures:
;	void js_end( void ) ;
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
;	MS-DOS
;
; Requiring Resources:
;	CPU: 
;
; Notes:
;	�L�[�o�b�t�@���N���A���邾���ł��B
;
; Assembly Language Note:
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
;	93/ 5/12 Initial: jsend.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc
	EXTRN	DOS_KEYCLEAR:CALLMODEL

	.CODE

func JS_END	; {
	call	DOS_KEYCLEAR
	ret
endfunc		; }

END
