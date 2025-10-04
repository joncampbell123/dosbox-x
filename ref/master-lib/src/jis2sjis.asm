; master library - kanji
;
; Description:
;	JIS漢字コードをSHIFT-JISコードに変換する
;
; Functions/Procedures:
;	unsigned jis_to_sjis( unsigned jis ) ;
;
; Parameters:
;	jis	JIS漢字コード
;
; Returns:
;	SHIFT-JIS漢字コード
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	JIS漢字以外のコードが渡された場合、結果は不定です。
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
;	94/12/21 Initial: jis2sjis.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE

func JIS_TO_SJIS	; jis_to_sjis() {
	mov	BX,SP
	; 引き数
	jiscode = (RETSIZE+0)*2

	mov	AX,SS:[BX+jiscode]
	test	AH,AH
	jle	short IGNORE	; foolproof

	sub	AX,0de82h
	rcr	AH,1
	jb	short SKIP
		cmp	AL,0deh
		sbb	AL,05eh
SKIP:	xor	AH,20h
IGNORE:
	ret	2
endfunc		; }

END
