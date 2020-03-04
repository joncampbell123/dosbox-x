; master library
;
; Global Variables:
;	unsigned font_AnkSeg	256パターン分の領域へのセグメント
;	unsigned font_AnkSize	super_patsize[]と同様の内容で、パターンサイズ
;	unsigned font_AnkPara	1文字に必要なパラグラフ数
;
; Revision History:
;	93/ 1/27 Initial
;	93/ 2/24 super.lib合併
;	94/ 1/24 font_AnkSize, font_AnkPara追加

	.MODEL SMALL
	.DATA?
	public font_AnkPara
	public _font_AnkPara
_font_AnkPara label word
font_AnkPara dw ?

	.DATA
	public font_AnkSeg
	public _font_AnkSeg
_font_AnkSeg label word
font_AnkSeg dw 0

	public font_AnkSize
	public _font_AnkSize
_font_AnkSize label word
font_AnkSize dw 0

END
