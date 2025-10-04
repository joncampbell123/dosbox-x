; master library - PC/AT - graph - text
;
; Description:
;	・グラフィックモードにもDOS/V emulation textの機能を付加する

;	vtext_colortable: DOS/V emulation text のエミュレーション用色変換配列
;	vtextx_Seg: vtextxに確保された仮想VRAMのセグメント
;	vtextx_Size: vtextxに確保された仮想VRAMの大きさ(TextVramSizeと同じ単位)
;
	.MODEL SMALL
	.DATA
	public vtext_colortable, _vtext_colortable
_vtext_colortable label byte
vtext_colortable db 0,9,0ch,0dh,0ah,0bh,0eh,0fh
		 db 8,1,4,5,2,3,6,7

	public vtextx_Seg, _vtextx_Seg
_vtextx_Seg label word
vtextx_Seg	dw	0

	public vtextx_Size, _vtextx_Size
_vtextx_Size label word
vtextx_Size	dw	0
END
