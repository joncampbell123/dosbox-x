; master library
;
; Description:
;	at98_scroll関連のグローバル変数定義
;
; Global Variables:
;	at98_VPage	表示頁
;	at98_APage	アクセスページ
;	at98_Offset	表示アドレスオフセット(スクロール状態保持用)
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	95/ 4/ 1 Initial: at98sc.asm / master.lib 0.22k
;
	.MODEL SMALL
	.DATA
	public _at98_VPage, at98_VPage
	public _at98_APage, at98_APage
	public _at98_Offset, at98_Offset
at98_APage	label word
_at98_APage	dw	0
at98_VPage	label word
_at98_VPage	dw	0
at98_Offset	label word
_at98_Offset	dw	0

END
