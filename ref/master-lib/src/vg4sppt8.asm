; superimpose & master library module
;
; Description:
;	VGA 16color, パターンの表示[横8dot単位]
;
; Functions/Procedures:
;	void vga4_super_put_8( int x, int y, int num ) ;
;
; Parameters:
;	x,y	描画する座標
;	num	パターン番号
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
;	94/ 4/ 5 Initial: vg4sppt8.asm/master.lib 0.23
;	94/ 5/26 [M0.23] BUGFIX GCのモードをCHARに設定していなかった
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD

	.CODE

func VGA4_SUPER_PUT_8	; vga4_super_put_8() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	CLD

	mov	CX,[BP+x]
	mov	AX,[BP+y]
	mul	graph_VramWidth

	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)

	mov	BP,CX
	shr	BP,3
	add	BP,AX		;GVRAM offset address

	mov	AH,0

	mov	AL,DL
	mov	CS:_YDOTS_,AX	; YDOTS

	mov	AL,byte ptr graph_VramWidth
	sub	AL,DH
	mov	CS:_ADD_DI_,AX

	; ごーごー
START:
	mov	ES,graph_VramSeg
	mov	DS,super_patdata[BX]
	xor	SI,SI

	mov	BL,DH		; xlen

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
	call	DISP		;originally cls_loop

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

;	mov	DX,SEQ_PORT
;	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
;	out	DX,AX

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

; DISP:
; in:
;	AX: out value
;	DS:SI = pattern address
;	ES:BP = draw top addres
;	BL = width(by byte)
;	(CX = height, set to _YDOTS_)
;
; break:
;	?
;
DISP	proc	near
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	DI,BP
	JMOV	CX,_YDOTS_
	JMOV	DX,_ADD_DI_
	mov	BH,BL
	shr	BH,1
	jz	short DISP_1
	jc	short DISP_ODD

	EVEN
PUT_LOOP1:
	lodsw
	test	ES:[DI],AL	; fill latch
	mov	ES:[DI],AL	; write
	test	ES:[DI+1],AH	; fill latch
	mov	ES:[DI+1],AH	; write
	add	DI,2
	dec	BH
	jnz	short PUT_LOOP1
	add	DI,DX
	mov	BH,BL
	shr	BH,1
	loop	short PUT_LOOP1
	ret

	EVEN
DISP_ODD:
PUT_LOOP2:
	lodsw
	test	ES:[DI],AL	; fill latch
	mov	ES:[DI],AL	; write
	test	ES:[DI+1],AH	; fill latch
	mov	ES:[DI+1],AH	; write
	add	DI,2
	dec	BH
	jnz	short PUT_LOOP2
	test	ES:[DI],AL	; fill latch
	movsb			; write
	add	DI,DX
	mov	BH,BL
	shr	BH,1
	loop	short PUT_LOOP2
	ret

	EVEN
DISP_1:
LOOP_1:
	test	ES:[DI],AL	; fill latch
	movsb			; write
	add	DI,DX
	loop	short LOOP_1
	ret

DISP	endp

END
