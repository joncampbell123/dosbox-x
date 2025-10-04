; master library - MS-DOS - 30BIOS
;
; Description:
;	30行BIOS制御
;	VSYNC周波数の取得
;
; Procedures/Functions:
;	unsigned bios30_getvsync( void ) ;
;
; Parameters:
;	
;
; Returns:
;	bios30_getvsync:
;		現在の実測のVSYNC周波数(未サポート||計測不能なら0x0000)
;		上位8ビットは整数部
;		下位8ビットは小数部
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30行BIOS 1.20以降で有効
;	この関数で使用している30行BIOS API ff0ahを呼び出すことにより
;	ビープ音周波数が初期化される
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	ちゅ～た
;
; Advice:
;	KEI SAKAKI.
;
; Revision History:
;	94/ 8/ 5  初期バージョン。
;

	.MODEL SMALL
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETVSYNC	; unsigned bios30_getvsync( void ) {
	_call	BIOS30_TT_EXIST		; 30行BIOS 1.20以降
	and	al,08h
	jz	short GETVSYNC_FAILURE

	mov	ax,0ff0ah		; VSYNC周波数の取得
	int	18h
GETVSYNC_FAILURE:
	ret
endfunc			; }


	END
