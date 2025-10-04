; master library - graphic - bytepixel - get - hline - 8dot - PC98V
;
; Description:
;	ドットごとに1バイトの水平グラフィックパターン読み取り
;
; Function/Procedures:
;	void graph_unpack_get_8( int x, int y, void far * linepat, int len ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	クリッピングは縦方向だけ gc_poly 相当の処理を行っています。
;	横は画面幅でクリッピングできます。
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

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XWIDTH equ 640
	SCREEN_XBYTE equ 80

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

func GRAPH_UNPACK_GET_8	; graph_unpack_get_8() {
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
	sub	AX,ClipYT
	cmp	AX,ClipYH
	ja	short CLIPOUT

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
	cmp	SI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,SI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,SI
	imul	AX,AX,SCREEN_XBYTE
	add	SI,AX		; SI = draw address

	; 描画開始
	push	DS
	mov	ES,[BP+linepat+2]

	mov	DS,ClipYT_seg

	mov	BP,CX
XLOOP:
	mov	BL,[SI]		; 0a800h	; BL = b
	mov	AX,DS
	mov	BH,[SI+8000h]	; 0b000h	; BH = r
	add	AX,1000h
	mov	DS,AX
	mov	DL,[SI]		; 0b800h	; DL = g
	add	AX,2800h
	mov	DS,AX
	mov	DH,[SI]		; 0e000h	; DH = e
	sub	AX,3800h
	mov	DS,AX
	inc	SI

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

	MRETURN
endfunc		; }

END
