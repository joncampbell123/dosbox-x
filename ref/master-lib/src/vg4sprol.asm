; superimpose & master library module
;
; Description:
;	VGA 16color, パターンの表示(画面上下接続)
;
; Functions/Procedures:
;	void vga4_super_roll_put( int x, int y, int num ) ;
;
; Parameters:
;	x,y	描画する座標
;	num	パターン番号
;
; Returns:
;	
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;	94/ 8/ 5 Initial: vg4sprol.asm / master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD
	EXTRN	graph_VramLines:WORD, graph_VramWords:WORD
	EXTRN	BYTE_MASK:BYTE

	.CODE

func VGA4_SUPER_ROLL_PUT	; vga4_super_roll_put() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	AX,graph_VramWidth
	mov	SI,[BP+y]
	imul	SI
	mov	DI,CX
	and	CX,7		; CX= x&7
	shr	DI,3
	add	DI,AX		; DI = y * graph_VramWidth + (x / 8)
	mov	CS:_DI_,DI

	mov	BX,CX
	mov	AL,BYTE_MASK[BX]

	mov	BX,[BP+num]
	add	BX,BX
	mov	DX,super_patsize[BX]

	push	super_patdata[BX]		; DS
	mov	BX,DX
	xor	BH,BH		; Bx = ysize
	add	SI,BX		; SI = y + ysize - graph_VramLines
	sub	SI,graph_VramLines
	jg	short noroll
	xor	SI,SI		; SI = 画面下にはみ出た長さ
noroll:
	sub	BX,SI		; BX = 画面内に描画できる長さ

	mov	CS:_YLEN2_,SI
	mov	CS:_YLEN1_,BX

	mov	CH,DH
	mov	DX,graph_VramWords
	add	DX,DX

	mov	AH,byte ptr graph_VramWidth
	sub	AH,CH
	shr	CH,1
	jc	short ODD_LEN

	mov	CS:VRAMSIZE1,DX
	mov	CS:BYTE_MASK1,AL
	mov	CS:ADD_DI1,AH
	mov	CS:COUNT1,CH
	mov	CS:DISP_ADDRESS,offset DISP_EVEN
	jmp	short START
	EVEN
ODD_LEN:
	mov	CS:VRAMSIZE2,DX
	mov	CS:BYTE_MASK2,AL
	mov	CS:ADD_DI2,AH
	mov	CS:COUNT2,CH
	mov	CS:DISP_ADDRESS,offset DISP_ODD

START:
	mov	ES,graph_VramSeg
	pop	DS			; load DS(pattern's segment)
	xor	SI,SI

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_SET_RESET_REG or (0 shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	call	DISP			; blank by mask

	mov	DX,VGA_PORT
	mov	AX,VGA_SET_RESET_REG or (0fh shl 8)
	out	DX,AX

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	DISP

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc				; }

DISP	proc near
	mov	DX,SEQ_PORT
	out	DX,AX
	JMOV	BX,_YLEN1_
	JMOV	DI,_DI_
	JMOV	BP,_YLEN2_
	mov	DL,0
	JMOV	AX,DISP_ADDRESS
	jmp	AX
	EVEN

	; 長さ偶数
ROLL_EVEN:
	sub	DI,7d00h
	org $-2
VRAMSIZE1	dw	?
	mov	BX,BP
	xor	BP,BP
	EVEN
	; 入り口はココ
DISP_EVEN:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
	org	$-1
BYTE_MASK1	db	?
	xor	DH,AL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	short DISP_EVEN
	test	ES:[DI],DL
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
	org	$-1
ADD_DI1	db	?
	mov	CH,11h	;dummy
	org	$-1
COUNT1	db	?
	dec	BX
	jnz	short DISP_EVEN
	or	BP,BP
	jnz	short ROLL_EVEN
	ret
	EVEN

	; 長さ奇数
ROLL_ODD:
	sub	DI,7d00h
	org $-2
VRAMSIZE2	dw	?
	mov	BX,BP
	xor	BP,BP
	EVEN
	; 入り口はココ
DISP_ODD:
	or	CH,CH
	jz	short skip2
	EVEN
put_loop2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
	org	$-1
BYTE_MASK2	db	?
	xor	DH,AL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	short put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	inc	DI
	test	ES:[DI],AH
	mov	ES:[DI],AH
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
	org	$-1
ADD_DI2		db	?
	mov	CH,11h	;dummy
	org	$-1
COUNT2		db	?
	dec	BX
	jnz	short DISP_ODD
	or	BP,BP
	jnz	short ROLL_ODD
	ret
DISP	endp

END
