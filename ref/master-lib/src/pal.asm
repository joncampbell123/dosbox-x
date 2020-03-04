; master library
;
; Description:
;	パレット関連のグローバル変数定義
;
; Global Variables:
;
;	PaletteTone	パレットの明るさ ( 0 ～ 100 )
;	PaletteNote	液晶モードフラグ
;	PalettesInit	パレットデータの初期値
;	Palettes	パレットデータ
;	ResPalSeg	常駐パレットのセグメントアドレス(0=未検査または不在)
;	SdiExists	SDIの存在(1=存在,0=不在)
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial
;	92/11/21 for Pascal
;	93/ 2/24 super.lib合併
;	93/ 4/ 6 PaletteLED追加
;	93/ 5/22 [M0.18] PaletteLED -> PaletteNoteに変更
;	93/ 5/29 [M0.18] .CONST->.DATA
;	93/12/ 5 [M0.22] PalettesInit をパレット幅8bitに対応

	.MODEL SMALL
	.DATA

	public _PaletteTone,PaletteTone
PaletteTone label word
_PaletteTone	dw 100

	public _PalettesInit,PalettesInit
PalettesInit label byte
		;	 R,   G,   B
_PalettesInit	db	000h,000h,000h	; 0
		db	000h,000h,0FFh	; 1
		db	0FFh,000h,000h	; 2
		db	0FFh,000h,0FFh	; 3
		db	000h,0FFh,000h	; 4
		db	000h,0FFh,0FFh	; 5
		db	0FFh,0FFh,000h	; 6
		db	0FFh,0FFh,0FFh	; 7
		db	077h,077h,077h	; 8
		db	000h,000h,0AAh	; 9
		db	0AAh,000h,000h	; 10
		db	0AAh,000h,0AAh	; 11
		db	000h,0AAh,000h	; 12
		db	000h,0AAh,0AAh	; 13
		db	0AAh,0AAh,000h	; 14
		db	0AAh,0AAh,0AAh	; 15

	public _PaletteNote,PaletteNote
PaletteNote label word
_PaletteNote	dw	0

	public _ResPalSeg,ResPalSeg
ResPalSeg label word
_ResPalSeg	dw	0

	public _SdiExists,SdiExists
SdiExists label word
_SdiExists	dw	0

	.DATA?

	public _Palettes,Palettes
Palettes label byte
_Palettes	db	48 dup (?)

END
