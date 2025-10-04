; master.lib
; vga graphic controler( 16color )
; 93/12/3

	; グラフィックコントローラ

VGA_PORT	equ 03ceh
VGA_SET_RESET_REG	equ 0	; setcolor
VGA_ENABLE_SR_REG	equ 1	; 
VGA_COLOR_CMP_REG	equ 2	; 
VGA_DATA_ROT_REG	equ 3	; setcolor
VGA_READPLANE_REG	equ 4	; ←readplaneは下位2bitにプレーン番号を書く!
VGA_MODE_REG		equ 5	; setcolorでVGA_CHARに設定される
VGA_MULTI_REG		equ 6	;
VGA_DISABLECMP		equ 7	;
VGA_BITMASK_REG		equ 8	; setcolorで0ffhに設定される

	; VGA_MODE_REGレジスタに設定する内容

VGA_READPLANE	equ 0h		; readmap選択regの場所をそのまま読む
VGA_COMPARE	equ 08h		; 読み込み時色比較レジスタと一致したらbit on

VGA_NORMAL	equ 0		; GCのモード
VGA_LATCH	equ 1
VGA_FILL	equ 2
VGA_CHAR	equ 3

	; VGA_DATA_ROT_REGレジスタに設定する内容

VGA_PSET	equ 00h
VGA_AND		equ 08h
VGA_OR		equ 10h
VGA_XOR		equ 18h


	; VGA Sequencer

SEQ_PORT	equ 03c4h
SEQ_RESET_REG		equ 0
SEQ_CLOCKING_MODE_REG	equ 1
SEQ_MAP_MASK_REG	equ 2	; value 0fh	setcolor
SEQ_CHARACTER_MAP_REG	equ 3
SEQ_MEMORY_MODE_REG	equ 4

	; backup/restore_video_mode用構造体
VIDEO_STATE struc
mode	    db	?	; 現在のビデオ・モード
rows	    db	?	; 現在の１画面の行数
cols	    db	?	; 現在の１行あたりの桁数
total_rows  db	?	; 現在の画面全体の行数
VIDEO_STATE ends
