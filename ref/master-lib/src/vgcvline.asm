; master library - VGA - 16color
;
; Description:
;	VGA 16color 垂直線の描画
;
; Functions/Procedures:
;	void vgc_vline( int x, int y1, int y2 ) ;

; Parameters:
;	int x	x座標
;	int y1	上のy座標
;	int y2	下のy座標
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・grc_setclip()によるクリッピングに対応しています。
;	・y1,y2の上下関係は逆でも動作します。
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
; 関連関数:
;	grc_setclip(), vgc_setcolor()
;
; Revision History:
;	93/12/ 3 Initial: vgcvline.asm/master.lib 0.22
;	94/ 4/ 8 [M0.23] 横640dot以外にも対応

	.186

	.MODEL SMALL

	.DATA?

	EXTRN	ClipXL:WORD, ClipXR:WORD
	EXTRN	ClipYT:WORD, ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE
	include func.inc
	include vgc.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

retfunc RETURN
	MRETURN
endfunc

func VGC_VLINE	; vgc_vline() {
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	x  = (RETSIZE+3)*2
	y1 = (RETSIZE+2)*2
	y2 = (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	CX,ClipYB

	mov	BX,[BP+y1]
	mov	DX,[BP+y2]
	cmp	BX,DX
	jl	short GO
	xchg	BX,DX		; BX <= DX にする
GO:

	; yのクリップ
	cmp	DX,CX
	jl	short BOTTOM_OK
	mov	DX,CX
BOTTOM_OK:
	cmp	BX,AX
	jg	short TOP_OK
	mov	BX,AX
TOP_OK:
	sub	DX,BX		; BX = y1, DX = y長さ
	jl	short RETURN

	mov	AX,[BP+x]	; xのクリップ
	cmp	AX,ClipXL
	jl	short RETURN
	cmp	AX,ClipXR
	jg	short RETURN
	mov	CX,AX
	and	CL,07h
	shr	AX,3
	mov	DI,AX

	mov	AL,80h
	shr	AL,CL

	mov	CX,DX		; CX = y長さ
	xchg	AX,BX
	mov	BP,graph_VramWidth	; BXが増分
	imul	BP
	add	DI,AX

	mov	ES,graph_VramSeg ; セグメント設定
	inc	CX		; 長さを出す(abs(y2-y1)+1)

	shr	CX,1
	jnb	short S1
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
S1:
	shr	CX,1
	jnb	short S2
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
S2:	jcxz	short QRETURN
	EVEN
L:	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	loop	short L

QRETURN:
	MRETURN
endfunc
END
