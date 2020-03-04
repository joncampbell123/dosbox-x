; master library - timer
;
; Description:
;	タイマ割り込み関連のグローバル変数定義
;
; Global Variables:
;	timer_Count
;	timer_Proc
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 3/20 Initial: ti.asm/master.lib 0.23
;
	.MODEL SMALL

	; バージョン文字列をリンクする
	EXTRN _Master_Version:BYTE

	.DATA
	public _timer_Count,timer_Count
timer_Count label dword
_timer_Count	dd	0

	public _timer_Proc,timer_Proc
timer_Proc label dword
_timer_Proc	dd	0

END
