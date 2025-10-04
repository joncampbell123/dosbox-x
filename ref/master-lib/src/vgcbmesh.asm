PAGE 98,120
; master library - VGA - SVGA - 16color - boxfill
;
; Description:
;	VGA/SVGA 16色 ドット市松模様の箱塗り [横8dot単位座標]
;
; Function:
;	void vgc_bytemesh_x( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	x1,y1	左上頂点	(x座標は0～79(横640dot時), y座標は0～)
;	x2,y2	右下頂点
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
;	・色をつけるには、vgc_setcolor()を利用してください。
;	・x1 <= x2, y1 <= y2 でなければ描画しません。
;	・クリッピングは縦方向のみ行っています。
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 4/ 8 Initial: vgcbyteb.asm/master.lib 0.23
;	94/ 4/ 8 Initial: vgcbmesh.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN ClipYT:WORD, ClipYB:WORD
	EXTRN graph_VramSeg:WORD, graph_VramWidth:WORD
	EXTRN graph_MeshByte:BYTE

	.CODE

MRETURN macro
	pop	BP
	ret	8
	EVEN
	endm

func VGC_BYTEMESH_X ; vgc_bytemesh_x() {
	push	BP
	mov	BP,SP
	; 引数
	x1	= (RETSIZE+4)*2
	y1	= (RETSIZE+3)*2
	x2	= (RETSIZE+2)*2
	y2	= (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	BX,[BP+y1]
	cmp	BX,AX
	jl	short TOP_OK
	mov	AX,BX
TOP_OK:
	mov	DX,[BP+y2]
	mov	CX,ClipYB
	cmp	DX,CX
	jg	short BOTTOM_OK
	mov	CX,DX
BOTTOM_OK:

	sub	CX,AX		; CX = ylen
	jl	short YCLIPOUT

	push	SI
	push	DI

	mov	DI,[BP+x1]
	mov	SI,[BP+x2]
	sub	SI,DI		; SI = xlen
	jl	short XCLIPOUT

	mov	BX,AX
	mov	BP,graph_VramWidth
	mul	BP
	add	DI,AX		; DI = ylen * VramWidth + x1

	mov	AL,graph_MeshByte
	test	BX,1
	jz	short S1
	not	AL
S1:
	mov	ES,graph_VramSeg
	inc	CX
	EVEN
YLOOP:
	mov	BX,SI
XLOOP:
	test	ES:[DI+BX],AL
	mov	ES:[DI+BX],AL
	dec	BX
	jns	short XLOOP
	not	AL
	add	DI,BP
	loop	short YLOOP
XCLIPOUT:
	pop	DI
	pop	SI
YCLIPOUT:
	MRETURN
endfunc			; }

END
