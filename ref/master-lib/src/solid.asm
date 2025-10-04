; public domain!!
; 単色を表す 8dot x 1lineのタイルパターン
; 16色分
;
; A.Koizuka
;
; Revision History:
;	92/ 7/16
;	93/ 5/29 [M0.18] .CONST->.DATA

	.MODEL SMALL
	.DATA

	public _SolidTile
		;	B    R    G    I
_SolidTile	db	000h,000h,000h,000h
		db	0ffh,000h,000h,000h
		db	000h,0ffh,000h,000h
		db	0ffh,0ffh,000h,000h
		db	000h,000h,0ffh,000h
		db	0ffh,000h,0ffh,000h
		db	000h,0ffh,0ffh,000h
		db	0ffh,0ffh,0ffh,000h
		db	000h,000h,000h,0ffh
		db	0ffh,000h,000h,0ffh
		db	000h,0ffh,000h,0ffh
		db	0ffh,0ffh,000h,0ffh
		db	000h,000h,0ffh,0ffh
		db	0ffh,000h,0ffh,0ffh
		db	000h,0ffh,0ffh,0ffh
		db	0ffh,0ffh,0ffh,0ffh

END
