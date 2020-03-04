; master library
;
; Global Variables:
;	void far * font_AnkFunc   CX=code,ES:SI=buf‚ÅŒÄ‚ÔAANK font“Ç‚İŠÖ”
;	void far * font_KanjiFunc CX=code,ES:SI=buf‚ÅŒÄ‚ÔAŠ¿šfont“Ç‚İŠÖ”
;
; Revision History:
;	94/ 7/27 Initial: fontat.asm / master.lib 0.23

	.MODEL SMALL

	.DATA
	public font_AnkFunc
	public _font_AnkFunc
_font_AnkFunc label dword
font_AnkFunc dd 0

	public font_KanjiFunc
	public _font_KanjiFunc
_font_KanjiFunc label dword
font_KanjiFunc dd 0

END
