; font_entry_cgrom()用文字コード変数
;
; 93/11/20

	.MODEL SMALL
	.DATA?

	public font_ReadChar,_font_ReadChar
_font_ReadChar label word
font_ReadChar	 dw ?

	public font_ReadEndChar,_font_ReadEndChar
_font_ReadEndChar label word
font_ReadEndChar dw ?

END
