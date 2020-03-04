; master library - VGA - 16color
;
; Description:
;	水平線の描画
;
; Functions/Procedures:
;	void vgc_hline( int xl, int xr, int y ) ;

; Parameters:
;	int xl	左端のx座標
;	int xr	右端のx座標
;	int y	y座標
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
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・grc_setclip()によるクリッピングに対応しています。
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
;	93/12/3 Initial: vgchline.asm/master.lib 0.22
;	94/3/9	Arranged by Ara
;	94/ 4/8 [M0.23] 横640dot以外にも対応

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA

	EXTRN	EDGES:WORD
	EXTRN	ClipXL:WORD, ClipXR:WORD
	EXTRN	ClipYT:WORD, ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

func VGC_HLINE	; vgc_hline() {
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	xl = (RETSIZE+3)*2
	xr = (RETSIZE+2)*2
	 y = (RETSIZE+1)*2

	mov	AX,[BP+y]
	cmp	AX,ClipYT
	jl	short RETURN	; y がクリップ範囲外 -> skip
	cmp	AX,ClipYB
	jg	short RETURN	; y がクリップ範囲外 -> skip

	mov	CX,[BP+xl]
	mov	BX,[BP+xr]

	imul	graph_VramWidth
	mov	BP,AX		; BP=VRAM ADDR

	cmp	BX,CX
	jl	short SKIP
	xchg	CX,BX		; BX <= CX
SKIP:
	mov	AX,ClipXL
	cmp	BX,AX
	jg	short LEFT_OK
	mov	BX,AX
LEFT_OK:
	mov	AX,ClipXR
	cmp	CX,AX
	jl	short RIGHT_OK
	mov	CX,AX
RIGHT_OK:
	sub	CX,BX		; CX = bitlen, BX = leftx
	jl	short RETURN	; 範囲外 → skip

	mov	ES,graph_VramSeg ; セグメント設定
	CLD

	mov	DI,BX		; addr = yaddr + xl / 8 ;
	shr	DI,3
	add	DI,BP

	and	BX,07h		; BX = xl & 7
	add	CX,BX
	sub	CX,8
	shl	BX,1
	mov	AL,byte ptr EDGES[BX]	; 左エッジ
	not	AL

	mov	BX,CX		;
	and	BX,7
	shl	BX,1

	sar	CX,3
	js	short LAST
	test	ES:[DI],AL
	stosb
	mov	AL,0ffh
	jcxz	short LAST
REPSTOSB:
	or	ES:[DI],AL
	inc	DI
	loop	short	REPSTOSB
LAST:	and	AL,byte ptr EDGES[BX+2]	; 右エッジ
	test	ES:[DI],AL
	mov	ES:[DI],AL
RETURN:
	MRETURN
endfunc		; }
END
