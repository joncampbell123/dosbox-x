; superimpose & master library module
; Description:
;	仮想VRAMのセグメントアドレスを保持する変数
;
; Variables:
;	virtual_seg
;
; Notes:
;	vircopy.asm の virtual_copy(),virtual_vram_copy(), 
;	vg4vcopy.asm の vga4_virtual_copy(),vga4_virtual_vram_copy()
;	などから使用されます。
;
; Revision History:
;	94/10/21 初期値が不定だったのを0に訂正

	.MODEL SMALL
	.DATA
	PUBLIC	virtual_seg,_virtual_seg
_virtual_seg label word
virtual_seg	DW	0

	PUBLIC	virtual_vramwords,_virtual_vramwords
_virtual_vramwords label word
virtual_vramwords	DW	0

END
