; master library - graphic - bytepixel - get - hline - 8dot - VGA
;
; Description:
;	ドットごとに1バイトの水平グラフィックパターン読み取り
;
; Function/Procedures:
;	void vga4_unpack_get_8( int x, int y, void far * linepat, int len ) ;
;
; Parameters:
;	x	描画開始 x 座標( 0～639, ただし8ドット単位に切り捨て )
;	y	描画 y 座標( 0 ～ 399(※400ライン表示のとき)
;	linepat	パターンデータの読み取り先の先頭アドレス
;	len	読み取る横ドット数
;		ただし実際には8dot単位に切り詰めます。
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	クリッピングは縦方向だけ gc_poly 相当の処理を行っています。
;	横は画面幅でクリッピングしています。
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 6/30 Initial: grpuget8.asm/master.lib 0.20
;	94/ 1/23 [M0.22] far pointer固定にした
;	94/ 5/26 Initial: vg4uget8.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
endm

retfunc CLIPOUT
	MRETURN
endfunc

func VGA4_UNPACK_GET_8	; vga4_unpack_get_8() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	linepat	= (RETSIZE+2)*2
	len	= (RETSIZE+1)*2

	mov	AX,[BP+y]
	cmp	AX,ClipYT
	jl	short CLIPOUT
	cmp	AX,ClipYB
	jg	short CLIPOUT

	mov	CX,[BP+len]
	sar	CX,3		; 8dot単位に切り捨てる
	jle	short CLIPOUT

	mov	DI,[BP+linepat]

	mov	SI,[BP+x]
	sar	SI,3		; xを8ドット単位に補正
	jns	short XNORMAL
	add	CX,SI
	jle	short CLIPOUT
	shl	SI,2
	add	DI,SI
	xor	SI,SI
XNORMAL:
	mov	BX,graph_VramWidth
	cmp	SI,BX
	jge	short CLIPOUT

	add	CX,SI
	cmp	CX,BX
	jl	short RIGHTCLIPPED
	mov	CX,BX
RIGHTCLIPPED:
	sub	CX,SI
	mul	BX
	add	SI,AX		; SI = draw address

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX

	; 描画開始
	push	DS
	mov	ES,[BP+linepat+2]

	mov	DS,graph_VramSeg

	mov	BP,CX
XLOOP:
	mov	DX,VGA_PORT
	mov	AX,VGA_READPLANE_REG or (0 shl 8)
	out	DX,AX
	mov	BL,[SI]		; BL = b
	inc	AH
	out	DX,AX
	mov	BH,[SI]		; BH = r
	inc	AH
	out	DX,AX
	mov	CL,[SI]		; CL = g
	inc	AH
	out	DX,AX
	mov	CH,[SI]		; CH = e
	sub	AX,3800h
	inc	SI
	mov	DX,CX		; DL=g, DH=e

	mov	CX,4
	EVEN
LOOP8:
	xor	AX,AX
	shl	DH,1
	rcl	AL,1
	shl	DL,1
	rcl	AL,1
	shl	BH,1
	rcl	AL,1
	shl	BL,1
	rcl	AL,1

	shl	DH,1
	rcl	AH,1
	shl	DL,1
	rcl	AH,1
	shl	BH,1
	rcl	AH,1
	shl	BL,1
	rcl	AH,1
	stosw
	loop	short LOOP8

	dec	BP
	jnz	short XLOOP

	pop	DS

	mov	DX,VGA_PORT
	mov	AX,VGA_READPLANE_REG or (0fh shl 8)
	out	DX,AX

	MRETURN
endfunc		; }

END
