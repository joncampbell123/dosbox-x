; master library - heap
;
; Description:
;	ヒープの中の最大フリーブロックのサイズを得る
;
; Function/Procedures:
;	unsigned hmem_maxfree(void) ;
;
; Parameters:
;	none
;
; Returns:
;	最大フリーブロックのパラグラフ数
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Assembly Language Note:
;	AX以外の全レジスタを保存します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 1 Initial: hmemmaxf.asm/master.lib 0.22
;	95/ 3/24 [M0.22k] BUGFIX smem_maxfree部が正しくなかった

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD
	EXTRN mem_EndMark:WORD

	.CODE

MEMHEAD STRUC
using	dw	?
nextseg	dw	?
MEMHEAD	ENDS

func HMEM_MAXFREE	; hmem_maxfree() {
	push	BX
	push	CX
	push	ES
	mov	AX,mem_TopHeap
	sub	AX,mem_EndMark
	mov	BX,AX			; BX = smem_maxfree()

	mov	AX,mem_TopHeap
	test	AX,AX
	jz	short DONE
FINDLOOP:
	mov	ES,AX
	cmp	ES:[0].using,0
	xchg	CX,AX			; mov CX,AX
	mov	AX,ES:[0].nextseg
	jne	short NEXT
	sub	CX,AX
	neg	CX
	cmp	BX,CX
	ja	short NEXT
	mov	BX,CX			; BX = max(BX,CX)
NEXT:	cmp	AX,mem_OutSeg
	jb	short FINDLOOP

DONE:
	lea	AX,[BX-1]
	clc
	pop	ES
	pop	CX
	pop	BX
	ret
endfunc		; }

END
