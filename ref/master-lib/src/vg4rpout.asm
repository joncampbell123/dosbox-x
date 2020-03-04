; master library module - VGA16Color
;
; Description:
;	VGA 16Color, 指定位置に表示されている指定キャラクタを消去する[高速版]
;
; Functions/Procedures:
;	void vga4_repair_out( int x, int y, int charnum ) ;
;
; Parameters:
;	x,y	左上端
;	charnum	既に表示されているキャラクタ番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 186
;	VGA
;
; Notes:
;	　横方向が8dot単位で復元されるので、左右が余分に大きく消去されます。
;	　指定パターンの大きさ丁度で消去するには、super_out()を使用して
;	下さい。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;$Id: repout.asm 0.02 92/05/29 20:11:56 Kazumi Rel $
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	94/11/ 6 Initial: vg4rpout.asm / master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD
	EXTRN	super_charnum:WORD, super_chardata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD

	.CODE

func VGA4_REPAIR_OUT	; vga4_repair_out() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	charnum	= (RETSIZE+1)*2

	CLD
	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
	out	DX,AX

	mov	CX,[BP+charnum]
	mov	AX,graph_VramWidth
	mov	BX,AX
	mul	word ptr [BP+y]
	mov	DX,[BP+x]
	mov	BP,AX
	mov	AX,BX		; AX = graph_VramWidth
	shr	DX,3		;DX=x/8
	add	BP,DX		;GVRAM offset address

	shl	CX,1		;integer size & near pointer
	mov	BX,CX
	mov	BX,super_charnum[BX]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	BX,CX
	xor	SI,SI
	mov	ES,graph_VramSeg
	mov	DS,super_chardata[BX]	;BX+2 -> BX
	mov	CL,DH
	xor	CH,CH
	inc	CX
	inc	CX		;1 byte
	and	CX,not 1
	mov	BX,AX		; graph_VramWidth
	sub	BL,CL
	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	REPAIR
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	call	REPAIR
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	REPAIR
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	REPAIR

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc				; }


REPAIR	proc	near
	mov	DI,DX
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	DX,DI

	mov	AX,CX

	mov	DI,BP
	mov	DH,DL
	shr	AX,1
	jnb	short REPAIR_EVEN

	EVEN
REPAIR_ODD:
	mov	CX,AX
	movsb
	rep	movsw
	lea	DI,[DI+BX]
	dec	DH
	jnz	short REPAIR_ODD
	rcl	AX,1
	mov	CX,AX
	ret

	EVEN
REPAIR_EVEN:
	mov	CX,AX
	rep	movsw
	add	DI,BX
	dec	DH
	jnz	short REPAIR_EVEN
	shl	AX,1
	mov	CX,AX
	ret
REPAIR	endp

END
