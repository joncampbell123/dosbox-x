; master library - vtext
;
; Description:
;	テキスト画面のカーソル位置取得
;
; Function/Procedures:
;	long vtext_getcurpos( void )
;
; Parameters:
;	none
;
; Returns:
;	long	HIWORD(DX) = y座標( 0 = 上端 )
;		LOWORD(AX) = x座標( 0 = 左端 )
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
;	PC/AT のワークエリア(page 0)を直接参照しています。
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
; Revision History:
;	93/07/28 Initial
;	93/08/10 関数名から _ を取る
;	94/01/16 関数名称変更(旧 get_cursor_pos_at)
;	94/ 4/ 9 Initial: vtgetcps.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE

func VTEXT_GETCURPOS	; vtext_getcurpos() {
	xor	AX,AX
	mov	ES,AX
	mov	DX,AX
	mov	AX,ES:[0450h]
	xchg	DL,AH
	ret
endfunc			; }

END
