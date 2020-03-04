; master library - PC98
;
; Description:
;	テキスト画面を表示する
;
; Function/Procedures:
;	void text_show( void ) ;
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
;	93/ 1/16 Initial
;	94/ 1/ 8 [M0.22] TextShownに設定

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextShown:WORD

	.CODE

func TEXT_SHOW
	mov	AH,0ch
	int	18h
	mov	TextShown,1
	ret
endfunc

END
