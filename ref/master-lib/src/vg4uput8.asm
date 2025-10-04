; master library - graphic - packedpixel - put - hline - 8dot - VGA - 16color
;
; Description:
;	VGA 16色, ドットごとに1バイトの水平グラフィックパターン表示
;
; Function/Procedures:
;	void vga4_unpack_put_8( int x, int y, void far * linepat, int len ) ;
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
;	VGA
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	クリッピングは縦方向だけ grc_setclip に対応しています。
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
;	93/ 6/23 Initial: grppput8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] large modelでのbugfix
;	93/ 7/ 3 [M0.20] クリップ枠最下行を外と判定していた(^^; fix
;	93/ 7/28 [M0.20] ちょっと加速
;	93/11/22 [M0.21] linepatをfar固定化
;	93/12/ 4 Initial: vg4pput8.asm/master.lib 0.22
;	94/ 6/25 Initial: vg4uput8.asm/amster.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

ROTATE	macro	wreg
	rept	2
	add	wreg,wreg
	ADC	CH,CH
	add	wreg,wreg
	ADC	CL,CL
	add	wreg,wreg
	ADC	BH,BH
	add	wreg,wreg
	ADC	BL,BL
	endm
endm

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

func VGA4_UNPACK_PUT_8	; vga4_unpack_put_8() {
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

	mov	SI,[BP+linepat]

	mov	DI,[BP+x]
	sar	DI,3		; xを8ドット単位に補正
	jns	short XNORMAL
	add	CX,DI
	jle	short CLIPOUT
	shl	DI,2
	add	SI,DI
	xor	DI,DI
XNORMAL:
	cmp	DI,graph_VramWidth
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,graph_VramWidth
	jl	short RIGHTCLIPPED
	mov	CX,graph_VramWidth
RIGHTCLIPPED:
	sub	CX,DI
	mul	graph_VramWidth
	add	DI,AX		; DI = draw address

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)	; mode0
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)	; pset
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)	; write directly
	out	DX,AX

	; 描画開始
	push	DS
	mov	ES,graph_VramSeg
	mov	DS,[BP+linepat+2]
	CLD

	mov	BP,CX
	EVEN
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

	mov	CX,DX
	mov	DX,SEQ_PORT

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	out	DX,AX
	mov	ES:[DI],BL	; b

	mov	AH,2
	out	DX,AX
	mov	ES:[DI],BH	; r

	mov	AH,4
	out	DX,AX
	mov	ES:[DI],CL	; g

	mov	AH,8
	out	DX,AX
	mov	ES:[DI],CH	; i
	inc	DI

	dec	BP
	jz	short owari
	jmp	XLOOP
owari:

	pop	DS

	MRETURN
endfunc		; }

END
