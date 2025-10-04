; master library - graphic - large - put - hline - 8dot - PC98V
;
; Description:
;	ドットごとに1バイトの水平グラフィックパターン表示(8dot単位, 2x2倍)
;
; Function/Procedures:
;	void graph_unpack_large_put_8( int x, int y, void far * linepat, int len ) ;
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
;	93/ 6/23 Initial: grppput8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] large modelでのbugfix
;	93/ 7/ 3 [M0.20] クリップ枠最下行を外と判定していた(^^; fix
;	93/ 7/28 [M0.20] ちょっと加速
;	93/11/22 [M0.21] linepatをfar固定化
;	93/12/ 8 [M0.22] RotTblを rottbl.asmに分離
;	94/ 2/15 Initial: grpulpt8.asm/master.lib 0.22a

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XBYTE equ 80

	.CODE

RotTbl2	dd	00000000h,00000003h,00000300h,00000303h
	dd	00030000h,00030003h,00030300h,00030303h
	dd	03000000h,03000003h,03000300h,03000303h
	dd	03030000h,03030003h,03030300h,03030303h

buf_a	dw	0
buf_b	dw	0

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

func GRAPH_UNPACK_LARGE_PUT_8	; graph_unpack_large_put_8() {
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
	shl	CX,1		; 2倍

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
	cmp	DI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,DI
	shr	CX,1		; データ側の長さに戻す
	imul	AX,AX,SCREEN_XBYTE
	add	DI,AX		; DI = draw address

	; 描画開始
	push	DS
	mov	ES,ClipYT_seg
	mov	DS,[BP+linepat+2]

	mov	BP,CX
	CLD
	EVEN
XLOOP:
	mov	CL,2

	mov	BL,[SI]
	mov	BH,0
	shl	BL,CL
	mov	AX,word ptr CS:RotTbl2[BX]
	mov	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	shl	BL,CL
	or	AX,word ptr CS:RotTbl2[BX]
	or	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	ENDM
	mov	CS:buf_a,AX
	mov	CS:buf_b,DX

	mov	BL,[SI]
	shl	BL,CL
	mov	AX,word ptr CS:RotTbl2[BX]
	mov	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	shl	BL,CL
	or	AX,word ptr CS:RotTbl2[BX]
	or	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	ENDM

	mov	CX,CS:buf_a
	xchg	CH,AL
	mov	ES:[DI],CX		; 0a800h
	mov	ES:[DI+SCREEN_XBYTE],CX
	mov	BX,ES
	mov	ES:[DI+8000h],AX	; 0b000h
	mov	ES:[DI+8000h+SCREEN_XBYTE],AX
	add	BH,10h

	mov	CX,CS:buf_b
	xchg	CH,DL
	mov	ES,BX
	mov	ES:[DI],CX		; 0b800h
	mov	ES:[DI+SCREEN_XBYTE],CX
	add	BH,28h
	mov	ES,BX
	mov	ES:[DI],DX		; 0e000h
	mov	ES:[DI+SCREEN_XBYTE],DX
	sub	BH,38h
	mov	ES,BX
	add	DI,2

	dec	BP
	ja	XLOOP

	pop	DS

	MRETURN
endfunc		; }

END
