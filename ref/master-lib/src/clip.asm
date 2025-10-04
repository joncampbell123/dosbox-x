; graphics - clipping
;
; DESCRIPTION:
;	描画クリップ枠の設定変数（初期値つき）
;
; GLOBAL VARIABLES:
;	int ClipXL, ClipXW, ClipXR	左端x, 枠の横幅(xr-xl), 右端x
;	int ClipYT, ClipYH, ClipYB	上端y, 枠の高さ(yb-yt), 下端y
;	unsigned ClipYB_adr		下端の左端のVRAMオフセット
;	unsigned ClipYT_seg		上端のVRAMセグメント(青プレーン)
;
; RUNNING TARGET:
;	NEC PC-9801 NORMAL MODE
;
; REFER:
;	  grc_setclip()
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; AUTHOR:
;	恋塚昭彦
;
; HISTORY:
;	92/5/16 Initial/gc_poly.lib
;	92/6/28 copyright追加
;	92/6/30 version 0.10
;	92/7/16 version 0.11
;	92/7/17 version 0.12
;	92/7/27 version 0.13
;	92/7/29 version 0.14
;	92/10/5 version 0.15
;	version 0.16
;	93/ 3/27 master.libに合併
;	93/ 6/12 [M0.19] バージョン文字列強制リンク

	.MODEL SMALL

	; バージョン文字列をリンクする
	EXTRN _Master_Version:BYTE

	.DATA

	public ClipXL, ClipXW, ClipXR, ClipYT, ClipYH, ClipYB
	public ClipYT_seg,ClipYB_adr
	public _ClipXL, _ClipXW, _ClipXR, _ClipYT, _ClipYH, _ClipYB
	public _ClipYT_seg,_ClipYB_adr
_ClipXL label word
ClipXL	dw 0	; 左端(0)
_ClipXW label word
ClipXW dw 639	; 幅(639)
_ClipXR label word
ClipXR dw 639	; 右端
_ClipYT label word
ClipYT dw 0	; てっぺん(0)
_ClipYH label word
ClipYH	dw 399	; 高さ(399)
_ClipYB label word
ClipYB dw 399	; 下端(399)
_ClipYT_seg label word
ClipYT_seg dw 0a800h	; 上端のVRAMセグメント(青プレーン上)
_ClipYB_adr label word
ClipYB_adr dw 399*80	; 下端のVRAMオフセット


	;台形データ
.DATA?
	public trapez_a,trapez_b
	public _trapez_a,_trapez_b
_trapez_a label word
trapez_a dw 4 dup(?)
_trapez_b label word
trapez_b dw 4 dup(?)

END
