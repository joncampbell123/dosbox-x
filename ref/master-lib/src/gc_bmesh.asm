; master library - GRCG
;
; Description:
;	ドット市松模様の箱塗り(横8dot単位, 青プレーンのみ)
;
; Functions/Procedures:
;	void grcg_bytemesh_x( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	x1,y1	左上頂点	(x座標は0～79, y座標は0～400(400line時))
;	x2,y2	右下頂点
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
;	GRCG
;
; Notes:
;	x1 <= x2, y1 <= y2 でなければ描画しません。
;	クリッピングは縦方向のみ行っています。
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
;	Kazumi(奥田  仁)
;
; Revision History:
;	93/ 5/ 4 Initial:gc_byteb.asm/master.lib 0.16
;	93/ 5/30 [M0.18] bugfix(^^; thanks >iR
;	94/ 4/ 6 Initial: gc_bmesh.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN ClipYT:WORD, ClipYH:WORD, ClipYT_seg:WORD
	EXTRN graph_MeshByte:BYTE

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	ret	8
	EVEN
	endm

func GRCG_BYTEMESH_X	; grcg_bytemesh_x() {
	push	SI
	push	DI
	mov	DI,SP
	; 引数
	x1	= (RETSIZE+5)*2
	y1	= (RETSIZE+4)*2
	x2	= (RETSIZE+3)*2
	y2	= (RETSIZE+2)*2

	mov	AX,ClipYT
	mov	CX,AX
	mov	BX,SS:[DI+y1]
	sub	BX,AX
	jg	short S1
	xor	BX,BX
S1:
	mov	AX,BX
	shl	AX,2
	add	AX,BX
	add	AX,ClipYT_seg
	mov	ES,AX

	mov	AX,ClipYH
	mov	DX,SS:[DI+y2]
	mov	SI,DX
	sub	DX,CX
	cmp	DX,AX
	jl	short S2
	mov	DX,AX
S2:
	sub	DX,BX		; (DX = ylen)
	jl	short OWARI

	mov	AX,SS:[DI+x1]
	mov	BX,SS:[DI+x2]
	sub	BX,AX		; BX = xlen
	jl	short OWARI
	inc	BX

	mov	DI,DX
	shl	DI,2
	add	DI,DX
	shl	DI,4
	add	DI,AX		; DI = ylen * 80 + x1

	lea	DX,[BX+80]	; DX = xsub
	mov	AL,graph_MeshByte
	mov	AH,AL
	and	SI,1
	jz	short SHIFT
	not	AX
	EVEN
SHIFT:

	test	DI,1
	jnz	short OA
	shr	BX,1
	jc	short EAOL
	EVEN
EAEL:
	mov	CX,BX
	rep	stosw
	not	AX
	sub	DI,DX
	jnb	short EAEL
OWARI:
	MRETURN
EAOL:
	mov	CX,BX
	rep	stosw
	stosb
	not	AX
	sub	DI,DX
	jnb	short EAOL
	MRETURN
OA:
	shr	BX,1
	jc	short OAOL
	dec	BX
	EVEN
OAEL:
	mov	CX,BX
	stosb
	rep	stosw
	stosb
	not	AX
	sub	DI,DX
	jnb	short OAEL
	MRETURN
OAOL:
	mov	CX,BX
	stosb
	rep	stosw
	not	AX
	sub	DI,DX
	jnb	short OAOL
	MRETURN
endfunc			; }

END
