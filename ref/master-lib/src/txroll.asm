; master library
; Description:
;	テキスト画面のスクロール範囲(for text_roll_*)
;
; Variables:
;	text_RollTopOff
;	text_RollHeight
;	text_RollWidth
;
; Revision History:
;	93/ 3/20 Initial

	.MODEL SMALL
	.DATA

	public text_RollTopOff,_text_RollTopOff
_text_RollTopOff label word
text_RollTopOff dw	0
	public text_RollHeight,_text_RollHeight
_text_RollHeight label word
text_RollHeight dw	24
	public text_RollWidth,_text_RollWidth
_text_RollWidth label word
text_RollWidth dw	80


END
