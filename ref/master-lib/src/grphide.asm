; master library - PC98
;
; Description:
;	グラフィック画面の表示を隠す
;
; Function/Procedures:
;	void graph_hide( void ) ;
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
;	92/11/16 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func GRAPH_HIDE
	mov	AH,41h
	int	18h
	ret
endfunc

END
