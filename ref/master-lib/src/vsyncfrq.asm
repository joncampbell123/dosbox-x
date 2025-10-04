; VSYNC周期をタイマカウンタ値で表した値
; Variables:
;	unsigned vsync_Freq ;	vga_vsync_start()で設定される。
;
;
	.MODEL SMALL
	.DATA?
	public vsync_Freq,_vsync_Freq
vsync_Freq label word
_vsync_Freq dw	?
END
