; master library - 
;
; Description:
;	�W���C�X�e�B�b�N�̃L�[�A�T�C���ύX
;
; Function/Procedures:
;	void js_key( unsigned func, int group, int maskbit ) ;
;
; Parameters:
;	func	
;	group	
;	maskbit	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
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
;	94/ 2/28 Initial: jskey.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

func JS_KEY	; js_key() {
	push	BP
	mov	BP,SP
	; ����
	_func   = (RETSIZE+3)*2
	_group  = (RETSIZE+2)*2
	maskbit = (RETSIZE+1)*2

	mov	BX,[BP+_func]
	mov	AL,[BP+maskbit]
	mov	CS:[BX+5],AL
	mov	AX,[BP+_group]
	mov	CS:[BX+2],AX
	pop	BP
	ret	6
endfunc		; }

END
