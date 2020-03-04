; master library - PC98V - GRCG
;
; Description:
;	画面の消去
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
;	現在のアクセス画面を消去します。
;	GRCGのタイルレジスタは all zeroに、モードは offにします。
;	割り込みを許可してしまいます。
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
	xor	DI,DI		; 消去開始ｵﾌｾｯﾄｱﾄﾞﾚｽ
	mov	CX,graph_VramWords
	mov	ES,graph_VramSeg
	rep	stosw		; 塗りつぶし
	mov	DI,BX		; restore DI

	out	7ch,AL		; GRCG OFF
	ret
endfunc

END
