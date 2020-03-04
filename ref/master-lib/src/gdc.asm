;
; Description:
;	GDC関連データ
;
; History:
;	92/ 7/27 Initial: gc_poly.lib
;	93/ 3/28 master.libに合併
;
.MODEL SMALL
.DATA
	public GDCUsed,_GDCUsed
_GDCUsed label word
GDCUsed	dw 0

	public GDC_LineStyle,_GDC_LineStyle
_GDC_LineStyle label word
GDC_LineStyle	dw 0FFFFh

	public GDC_Color,_GDC_Color
_GDC_Color label word
GDC_Color	dw 7	; pset,7

	public GDC_AccessMask,_GDC_AccessMask
_GDC_AccessMask label word
GDC_AccessMask	dw 0	; all
end
