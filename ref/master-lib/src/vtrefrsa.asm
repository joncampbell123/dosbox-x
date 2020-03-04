; master library - vtext
;
; Description:
;	仮想テキスト画面を再描画する。
;
; Procedures/Functions:
;	vtext_refresh_all( void )
;	vtext_refresh_on( void )
;	vtext_refresh_off( void )
;
; Parameters:
;	
;
; Returns:
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
;	OPTASM 1.6
;
; Author:
;	のろ/V(H.Maeda)
;
; Revision History:
;	93/10/10 Initial
;	93/11/ 7 vtext_refresh_on, _off 追加
;	94/ 4/ 9 Initial: vtrefrsa.asm/master.lib 0.23

	.MODEL SMALL
	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramSize : WORD
	EXTRN VTextState : WORD

	include func.inc

	.CODE

func VTEXT_REFRESH_ALL
	push	DI

	les	DI,TextVramAdr
	mov	CX,TextVramSize
	mov	AH,0ffh
	int	10h	; func. refresh

	pop	DI
	ret
endfunc

func VTEXT_REFRESH_ON
	and	VTextState,0fffeh
	ret
endfunc

func VTEXT_REFRESH_OFF
	or	VTextState,0001h
	ret
endfunc

END
