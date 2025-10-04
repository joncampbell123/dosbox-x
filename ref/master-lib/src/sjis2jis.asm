; master library - kanji
;
; Description:
;	SHIFT-JIS漢字コードをJISコードに変換する
;
; Functions/Procedures:
;	unsigned sjis_to_jis( unsigned sjis ) ;
;
; Parameters:
;	sjis	SHIFT JIS漢字コード
;
; Returns:
;	JIS漢字コード
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
;	SHIFT-JIS漢字以外のコードが渡された場合、結果は不定です。
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
;	94/12/21 Initial: sjis2jis.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE

func SJIS_TO_JIS	; sjis_to_jis() {
	mov	BX,SP
	; 引き数
	sjiscode = (RETSIZE+0)*2

	mov	AX,SS:[BX+sjiscode]
	test	AX,AX
	jns	short IGNORE	; foolproof

	shl	AH,1
	cmp	AL,9fh
	jnb	short SKIP
		cmp	AL,80h
		adc	AX,0fedfh
SKIP:	sbb	AX,0dffeh		; -(20h+1),-2
	and	AX,07f7fh
IGNORE:
	ret	2
endfunc		; }

END
