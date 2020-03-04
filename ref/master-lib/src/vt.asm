; master library module
; Description:
;	VTEXT関連変数
;
; Variables:
;	TextVramSize:
;		テキスト画面全体の文字数, ただしAT互換機専用
;
;	VTextState:
;		状態変数
;		bit  0: 0=書き込み即描画      1=自動的に描画は行わない
;		bit 14: 0=普通                1=テキストグラフィックモード※
;		bit 15: 0=描画許可            1=描画禁止(グラフィックモード)
;
;		※テキストグラフィックモード:
;		   DOS/Vのテキスト表示をmaster.libのvtextx_startによって
;		 グラフィックモードでエミュレーションしている状態

; Revision History:
;	94/ 4/ 9 Initial: vt.asm/master.lib 0.23

	.MODEL SMALL
	.DATA
	public _TextVramSize,TextVramSize
	public _VTextState,VTextState

TextVramSize label word
_TextVramSize dw 4000

VTextState label word
_VTextState dw 0

END
