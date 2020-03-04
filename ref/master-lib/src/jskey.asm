; master library - 
;
; Description:
;	ジョイスティックのキーアサイン変更
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
;	恋塚昭彦
;
; Revision History:
;	94/ 2/28 Initial: jskey.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

func JS_KEY	; js_key() {
	push	BP
	mov	BP,SP
	; 引数
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
