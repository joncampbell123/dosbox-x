; graphics - clipping
;
; DESCRIPTION:
;	描画クリップ枠の設定処理
;
; FUNCTION(in C):
;	int far _pascal grc_setclip( int xl, int yt, int xr, int yb ) ;
;
; FUNCTION(in Pascal):
;	function grc_setclip( xl,yt,xr,yb : integer ) : integer ;
;
; PARAMETERS:
;	int xl	左端のx座標
;	int yt	上端のy座標
;	int xr	右端のx座標
;	int yb	下端のy座標
;
; RETURNS:
;	0 = 失敗。実画面(640x400)の外にクリップ領域を設定しようとした
;	1 = 成功。
;
; NOTES:
;	・入力は、まず画面枠(0,0)-(639,399)でクリップしている。
;	・入力の対は、大小関係が逆でも揃えてから処理している。
;
; GLOBAL VARIABLES:
;	int ClipXL, ClipXW, ClipXR	左端x, 枠の横幅(xr-xl), 右端x
;	int ClipYT, ClipYH, ClipYB	上端y, 枠の高さ(yb-yt), 下端y
;	unsigned ClipYB_adr		下端の左端のVRAMオフセット
;	unsigned ClipYT_seg		上端のVRAMセグメント(青プレーン)
;
; REFER:
;	・対応関数
;	  grcg_polygon_convex()
;	  grcg_trapezoid()
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; AUTHOR:
;	恋塚昭彦
;
; HISTORY:
;	92/5/20 Initial
;	92/6/5	_pascal化したが、バグ出た。ひー。直した。ぶう
;	92/6/5	ClipYB_adr追加。
;	92/6/6	ClipYT_seg, GRamStart追加。
;	92/6/16 データ部分をclip.asmに分離
;	93/3/27 master.libに合併
;	93/12/28 [M0.22] 横の制限はgraph_VramWidth*8ドットにした
;

.186
.MODEL SMALL
.DATA
	extrn ClipXL:word, ClipXW:word, ClipXR:word
	extrn ClipYT:word, ClipYH:word, ClipYB:word
	extrn ClipYT_seg:word, ClipYB_adr:word
	extrn graph_VramSeg:word
	extrn graph_VramLines:word
	extrn graph_VramWidth:word	; 16の倍数であること!!

.CODE
	include func.inc

;	int far _pascal grc_setclip( int xl, int yt, int xr, int yb ) ;
func GRC_SETCLIP
	push	BP
	mov	BP,SP

	; 引数
	xl = (RETSIZE+4)*2
	yt = (RETSIZE+3)*2
	xr = (RETSIZE+2)*2
	yb = (RETSIZE+1)*2

;------------------------- Ｘ座標
	mov	AX,[BP+xl]
	mov	BX,[BP+xr]

	test	AX,BX		; AX < 0 && BX < 0 ならエラー
	js	short ERROR

	cmp	AX,BX
	jl	short A
	xchg	AX,BX		; AX <= BXにする
A:
	cmp	AX,8000h	;
	sbb	DX,DX		;
	and	AX,DX		; AXが負数ならゼロにする

	mov	CX,graph_VramWidth
	shl	CX,3
	dec	CX
	sub	BX,CX		;
	sbb	DX,DX		;
	and	BX,DX		;
	add	BX,CX		; BXが639以上なら639にする

	sub	BX,AX
	jl	short ERROR

	mov	ClipXL,AX
	mov	ClipXW,BX
	add	AX,BX
	mov	ClipXR,AX

;------------------------- Ｙ座標

	mov	AX,[BP+yt]
	mov	BX,[BP+yb]

	test	AX,BX		; AX < 0 && BX < 0 ならエラー
	js	short ERROR

	cmp	AX,BX
	jl	short B
	xchg	AX,BX		; AX <= BXにする
B:
	cmp	AX,8000h	;
	sbb	DX,DX		;
	and	AX,DX		; AXが負数ならゼロにする

	mov	CX,graph_VramLines
	dec	CX
	sub	BX,CX		;
	sbb	DX,DX		;
	and	BX,DX		;
	add	BX,CX		; BXが399以上なら399にする

	sub	BX,AX
	jl	short ERROR

	mov	ClipYT,AX
	mov	CX,AX
	mov	ClipYH,BX
	add	AX,BX
	mov	ClipYB,AX

	mov	AX,graph_VramWidth
	xchg	AX,BX
	mul	BX
	mov	ClipYB_adr,AX	; 下端yの左端のVRAMオフセット
				; (ClipYT_segセグメント上)

	mov	AX,BX
	shr	AX,4
	mul	CX
	add	AX,graph_VramSeg ; 開始セグメント = graph_VramSeg
	mov	ClipYT_seg,AX	;                 + yt * graph_VramWidth/16

	mov	AX,1
	pop	BP
	ret	8

ERROR:
	xor	AX,AX
	pop	BP
	ret	8
endfunc
END
