; master library - VGA 16Color
;
; Description:
;	グラフィック画面 横8dot単位の画面領域保存/復元
;
; Function/Procedures:
;	保存
;	void vga4_byteget( int cx1,int y1, int cx2,int y2, void far *buf ) ;
;
;	復元
;	void vga4_byteput( int cx1,int y1, int cx2,int y2, const void far *buf ) ;
;
; Parameters:
;	int cx1,cx2 : 0～graph_VramWidth-1
;	int y1,y2 : 0～graph_VramLines-1
;
; Returns:
;	None
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
;	クリッピングしてないよう
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 2/ 5 Initial: grpbget.asm
;	94/ 6/ 6 Initial: vg4bget.asm / master.lib 0.23
;
	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramWidth:WORD

	.CODE

func VGA4_BYTEGET	; vga4_byteget() {
	push	BP
	mov	BP,SP
	sub	SP,6

	; 引数
	cx1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	cx2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	plane	= -2
	gadr	= -4
	gwid	= -6

	push	SI
	push	DI
	push	DS

	mov	SI,[BP+cx1]	; もし cx1 > cx2 なら、
	mov	CX,[BP+cx2]	;	cx1 <-> cx2
	cmp	SI,CX		;
	jle	short GET_SKIP1	;
	xchg	SI,CX		;
GET_SKIP1:			; SI = cx1
	sub	CX,SI		; CX = cx2 - SI + 1
	inc	CX		;

	mov	AX,[BP+y1]	; AX = y1
	mov	BX,[BP+y2]
	cmp	BX,AX
	jg	short GET_SKIP2
	xchg	AX,BX
GET_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	DX,graph_VramWidth
	mov	[BP+gwid],DX
	mul	DX		; SI += y1 * 80
	add	SI,AX		;
	mov	[BP+gadr],SI

	les	DI,[BP+buf]

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX
	mov	AX,VGA_READPLANE_REG or (0 shl 8)
	mov	DS,graph_VramSeg

GET_LOOP:	; プレーンのループ
	mov	DX,VGA_PORT
	out	DX,AX
	mov	[BP+plane],AX

	; ES:DIの正規化
	mov	AX,DI
	and	DI,0fh
	shr	AX,1
	shr	AX,1
	shr	AX,1
	shr	AX,1
	mov	DX,ES
	add	DX,AX
	mov	ES,DX

	mov	AX,BX
	mov	DX,CX
GET_LOOP2:	; 縦のループ
	shr	CX,1
	rep movsw
	adc	CX,CX
	rep movsb
	mov	CX,DX
	sub	SI,DX
	add	SI,[BP+gwid]
	dec	AX
	jnz	short GET_LOOP2

	mov	SI,[BP+gadr]

	mov	AX,[BP+plane]
	inc	AH
	cmp	AH,4
	jl	short GET_LOOP

	pop	DS
	pop	DI
	pop	SI
	leave
	ret	6*2
endfunc		; }

func VGA4_BYTEPUT ; vga4_byteput() {
	push	BP
	mov	BP,SP
	sub	SP,6

	; 引数
	cx1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	cx2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2
	plane	= -2
	gadr	= -4
	gwid	= -6

	push	SI
	push	DI
	push	DS

	mov	DI,[BP+cx1]	; もし cx1 > cx2 なら、
	mov	CX,[BP+cx2]	;	cx1 <-> cx2
	cmp	DI,CX		;
	jle	short PUT_SKIP1	;
	xchg	DI,CX		;
PUT_SKIP1:
	sub	CX,DI		; DI = cx1
	inc	CX		; CX = cx2 - DI + 1

	mov	AX,[BP+y1]	;
	mov	BX,[BP+y2]	;
	cmp	AX,BX		;
	jle	short PUT_SKIP2	; もし y1 > y2 なら、
	xchg	AX,BX		;	y1 <-> y2
PUT_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	DX,graph_VramWidth
	mov	[BP+gwid],DX
	mul	DX		; DI += y1 * graph_VramWidth
	add	DI,AX		;
	mov	[BP+gadr],DI

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
	out	DX,AX

	mov	AX,SEQ_MAP_MASK_REG or (01h shl 8)
	mov	ES,graph_VramSeg
	lds	SI,[BP+buf]
PUT_LOOP:
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	[BP+plane],AX

	; DS:SIの正規化
	mov	AX,SI
	and	SI,0fh
	shr	AX,1
	shr	AX,1
	shr	AX,1
	shr	AX,1
	mov	DX,DS
	add	DX,AX
	mov	DS,DX

	mov	AX,BX
	mov	DX,CX
PUT_LOOP2:
	shr	CX,1
	rep movsw
	adc	CX,CX
	rep movsb
	mov	CX,DX
	sub	DI,DX
	add	DI,[BP+gwid]
	dec	AX
	jne	short PUT_LOOP2

	mov	DI,[BP+gadr]
	mov	AX,[BP+plane]
	shl	AH,1
	cmp	AH,10h
	jl	short PUT_LOOP

;	mov	DX,SEQ_PORT
;	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
;	out	DX,AX

	pop	DS
	pop	DI
	pop	SI
	leave
	ret	6*2
endfunc	; }

END
