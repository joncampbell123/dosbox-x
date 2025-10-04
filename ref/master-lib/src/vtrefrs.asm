; master library - vtext
;
; Description:
;	仮想テキスト画面の再描画をする。
;
; Function/Procedures:
;	void vtext_refresh( int x, int y, int len ) ;
;
; Parameters:
;	x,y	開始座標
;	len	strings 長
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
; Revision History:
;	93/10/10 Initial
;	94/ 4/ 9 Initial: vtrefrs.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramWidth : WORD
	EXTRN Machine_State : WORD

	.CODE

func VTEXT_REFRESH	; vtext_refresh() {
	push	BP
	mov	BP,SP

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	len	= (RETSIZE+1)*2

	push	DI

	mov	AX,[BP+y]
	mov	BX,TextVramWidth
	mul	BX	; DX:AX= y * Width

	mov	BX,[BP+x]
	add	AX,BX
	shl	AX,1	; (y1 * 80 + x1) * 2
	les	DI,TextVramAdr
	add	DI,AX

	mov	CX,[BP+len]

	mov	AH,0ffh
	int	10h	; func. refresh

	pop	DI

	pop	BP
	ret	6
endfunc			; }

END
