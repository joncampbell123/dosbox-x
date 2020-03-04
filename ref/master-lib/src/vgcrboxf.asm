; master library - VGA - round - boxfill
;
; Description:
;	角の丸い四角形塗りつぶし (level 2)
;
; Functions/Procedures:
;	void vgc_round_boxfill( int x1, int y1, int x2, int y2, unsigned r ) ;

; Parameters:
;	int x1,y1	左上角の座標
;	int x2,y2	右下角の座標
;	unsigned r	四隅の円の半径
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	・描画規則は vgc_hline, vgc_boxfillに依存しています。
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・grc_setclip()によるクリッピングに対応しています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;	vgc_hline(), vgc_boxfill()
;
; Revision History:
;	93/ 3/ 1 Initial
;	94/ 4/10 Initial: vgcrboxf.asm/master.lib 0.23

	.186
	.MODEL SMALL
	.CODE
	include func.inc
		EXTRN	VGC_HLINE:CALLMODEL
		EXTRN	VGC_BOXFILL:CALLMODEL

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	10
	endm

	; 引数
	x1	= (RETSIZE+5)*2
	y1	= (RETSIZE+4)*2
	x2	= (RETSIZE+3)*2
	y2	= (RETSIZE+2)*2
	r	= (RETSIZE+1)*2

retfunc BOXFILL
	push	word ptr [BP+x1]
	push	word ptr [BP+y1]
	push	word ptr [BP+x2]
	push	word ptr [BP+y2]
	call	VGC_BOXFILL

	MRETURN
endfunc

func VGC_ROUND_BOXFILL	; vgc_round_boxfill() {
	enter	2,0
	push	SI
	push	DI

	; 変数
	s	= -2

	mov	AX,[BP+r]

	mov	BX,[BP+x1]
	mov	SI,[BP+x2]
	cmp	BX,SI
	jle	short SKIP1
	xchg	BX,SI
	mov	[BP+x1],BX
	mov	[BP+x2],SI
SKIP1:
	sub	SI,BX
	shr	SI,1	; SI = abs(x2 - x1) / 2

	sub	AX,SI	; AX = min(AX,SI)
	sbb	DX,DX
	and	AX,DX
	add	AX,SI

	mov	CX,[BP+y1]
	mov	DI,[BP+y2]
	cmp	CX,DI
	jle	short SKIP2
	xchg	CX,DI
SKIP2:
	mov	BX,DI	; BX = max(y1,y2)
	sub	DI,CX
	shr	DI,1	; DI = abs(y2 - y1) / 2

	sub	AX,DI	; AX = min(AX,DI)
	sbb	DX,DX
	and	AX,DX
	add	AX,DI
	jz	short	BOXFILL	; 半径が 0 になったら長方形になる
	mov	[BP+s],AX

	add	CX,AX		; y1,y2を内側へずらす
	sub	BX,AX
	mov	[BP+y1],CX
	mov	[BP+y2],BX

	mov	SI,AX		; SI = wx

	inc	CX
	dec	BX
	cmp	BX,CX
	jl	short SKIP_BOXFILL

	push	[BP+x1]
	push	CX
	push	[BP+x2]
	push	BX
	call	VGC_BOXFILL
SKIP_BOXFILL:

	add	[BP+x1],SI
	sub	[BP+x2],SI
	mov	DI,0		; DI = wy
LOOP_8:
	mov	AX,[BP+x1]
	sub	AX,SI
	push	AX	; x1 - wx
	mov	AX,[BP+x2]
	add	AX,SI
	push	AX	; x2 + wx
	mov	AX,[BP+y1]
	sub	AX,DI	; y1 - wy
	push	AX
	call	VGC_HLINE

	mov	AX,[BP+x1]
	sub	AX,SI
	push	AX	; x1 - wx
	mov	AX,[BP+x2]
	add	AX,SI
	push	AX	; x2 + wx
	mov	AX,[BP+y2]
	add	AX,DI	; y1 + wy
	push	AX
	call	VGC_HLINE

	mov	AX,DI
	stc
	rcl	AX,1	; (wy << 1) + 1
	sub	[BP+s],AX
	jns	short LOOP_8E

	mov	AX,[BP+x1]
	sub	AX,DI
	push	AX	; x1 - wy
	mov	AX,[BP+x2]
	add	AX,DI
	push	AX	; x2 + wy
	mov	AX,[BP+y1]
	sub	AX,SI	; y1 - wx
	push	AX
	call	VGC_HLINE

	mov	AX,[BP+x1]
	sub	AX,DI
	push	AX	; x1 - wy
	mov	AX,[BP+x2]
	add	AX,DI
	push	AX	; x2 + wy
	mov	AX,[BP+y2]
	add	AX,SI	; y1 + wx
	push	AX
	call	VGC_HLINE

	dec	SI		; s += --wx << 1 ;
	mov	AX,SI
	shl	AX,1
	add	[BP+s],AX

LOOP_8E:
	inc	DI
	cmp	SI,DI
	jae	short	LOOP_8

	MRETURN
endfunc			; }

END
