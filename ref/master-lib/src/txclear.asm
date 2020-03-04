; master library - PC98 - MSDOS
;
; Description:
;	テキスト画面を消去する(高速コンソール出力仕様)
;
; Function/Procedures:
;	void text_clear( void ) ;
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
;	92/11/15 Initial
;	93/ 4/25 int DChから int 29Hに変更

	.MODEL SMALL
	include func.inc
	.CODE

func TEXT_CLEAR
	mov	AL,27	; 高速コンソール出力
	int	29h
	mov	AL,'['
	int	29h
	mov	AL,'2'
	int	29h
	mov	AL,'J'
	int	29h
	ret
endfunc

END
