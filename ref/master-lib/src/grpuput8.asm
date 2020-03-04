; master library - graphic - packedpixel - put - hline - 8dot - PC98V
;
; Description:
;	ドットごとに1バイトの水平グラフィックパターン表示
;
; Function/Procedures:
;	void graph_unpack_put_8( int x, int y, void far * linepat, int len ) ;
;
; Parameters:
;	x	描画開始 x 座標( 0～639, ただし8ドット単位に切り捨て )
;	y	描画 y 座標( 0 ～ 399(※400ライン表示のとき)
;	linepat	パターンデータの先頭アドレス
;	len	パターンデータの横ドット数
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
;	93/ 6/30 Initial: grpuput8.asm/master.lib 0.20
;	93/11/ 5 far固定に変更

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

func GRAPH_UNPACK_PUT_8	; graph_unpack_put_8() {
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

	mov	SI,[BP+linepat]

	mov	DI,[BP+x]
	cmp	DI,SCREEN_XWIDTH
	sar	DI,3		; xを8ドット単位に補正
	jns	short XNORMAL
	add	CX,DI
	jle	short CLIPOUT
	shl	DI,2
	add	SI,DI
	xor	DI,DI
XNORMAL:
	cmp	DI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,DI
	imul	AX,AX,SCREEN_XBYTE
	add	DI,AX		; DI = draw address

	; 描画開始
	push	DS
	mov	ES,ClipYT_seg
	mov	DS,[BP+linepat+2]
	CLD

	mov	BP,CX
XLOOP:
	mov	CX,4
	EVEN
LOOP8:
	lodsw
	shr	AL,1
	rcl	BL,1
	shr	AL,1
	rcl	BH,1
	shr	AL,1
	rcl	DL,1
	shr	AL,1
	rcl	DH,1

	shr	AH,1
	rcl	BL,1
	shr	AH,1
	rcl	BH,1
	shr	AH,1
	rcl	DL,1
	shr	AH,1
	rcl	DH,1
	loop	short LOOP8

	mov	ES:[DI],BL		; 0a800h
	mov	AX,ES
	mov	ES:[DI+8000h],BH	; 0b000h
	add	AX,1000h
	mov	ES,AX
	mov	ES:[DI],DL		; 0b800h
	add	AX,2800h
	mov	ES,AX
	mov	ES:[DI],DH		; 0e000h
	sub	AX,3800h
	mov	ES,AX
	inc	DI

	dec	BP
	jnz	short XLOOP

	pop	DS

	MRETURN
endfunc		; }

END
