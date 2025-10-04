; superimpose & master library module
;
; Description:
;	VGA 16color, パターンの表示
;
; Functions/Procedures:
;	void vga4_super_put( int x, int y, int num ) ;
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
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superput.asm 0.17 92/05/29 20:38:10 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]
;	93/ 9/20 [M0.21] すこし(32byteぐらい)縮小
;	93/ 9/20 [M0.21] WORD_MASK廃止
;	93/12/ 8 Initial: vg4spput.asm/master.lib 0.22
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD
	EXTRN	BYTE_MASK:BYTE

	.CODE

MRETURN	macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
	endm

retfunc EXIT
	MRETURN
endfunc

func VGA4_SUPER_PUT	; vga4_super_put() {
dummlabel label near
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
	test	DX,DX
	jz	short EXIT

	mov	BP,CX
	shr	BP,3
	add	BP,AX		;GVRAM offset address

	and	CX,7
	mov	SI,CX

	mov	AL,DL
	mov	AH,BYTE_MASK[SI]
	mov	CS:_BX_,AX	; YDOTS

	mov	CH,DH		;DL -> DH
	shr	CH,1
	jc	short ODD_SIZE1

	; 長さが偶数
	mov	BYTE PTR CS:[COUNT1],CH
	mov	AL,byte ptr graph_VramWidth
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI1],AL
	mov	CS:DISP_ADDRESS,offset DISP1 - offset JUMP_ORIGIN
	jmp	short START

	; 長さが奇数
	EVEN
ODD_SIZE1:
	mov	BYTE PTR CS:[COUNT2],CH
	mov	AL,byte ptr graph_VramWidth
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI2],AL
	mov	CS:DISP_ADDRESS,offset DISP2 - offset JUMP_ORIGIN


	; ごーごー
START:
	mov	ES,graph_VramSeg
	mov	DS,super_patdata[BX]
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

	MRETURN
endfunc			; }

; DISP:
; in:
;	AX: out value
;	DX: port
;	DS:SI = pattern address
;	ES:BP = draw top addres
;	CL = shift count
;	CH = width(by word), equals to COUNT1 or 2
;	(BX = height, set to _BX_)
;
; break:
;	AX,DI,DX
;

; 長さ偶数
;
DISP1	proc	near
	xor	DL,DL
	EVEN
PUT_LOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL
;	jz	short P1S
	test	ES:[DI],AL	; fill latch
	mov	ES:[DI],AL	; write
P1S:
	test	ES:[DI+1],AH	; fill latch
	mov	ES:[DI+1],AH	; write
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP1
	test	ES:[DI],DL	; fill latch
	mov	ES:[DI],DL	; write
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI1		EQU	$-1
	mov	CH,11h	;dummy
COUNT1		EQU	$-1
	dec	BL
	jnz	short PUT_LOOP1
	ret
DISP1	endp

; 長さ奇数
;
	EVEN
DISP2	proc	near
	xor	DL,DL
	EVEN
SINGLE_CHECK2:
	or	CH,CH
	jz	short SKIP2

	EVEN
PUT_LOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,BH
	xor	DH,AL
	or	AL,DL
;	jz	short P2S
	test	ES:[DI],AL	; fill latch
	mov	ES:[DI],AL	; write
P2S:
	test	ES:[DI+1],AH	; fill latch
	mov	ES:[DI+1],AH	; write
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP2

SKIP2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
;	jz	short P2S2
	test	ES:[DI],AL	; fill latch
	mov	ES:[DI],AL	; write
P2S2:
	inc	DI
	test	ES:[DI],AH	; fill latch
	mov	ES:[DI],AH	; write
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI2		EQU	$-1
	mov	CH,11h	;dummy
COUNT2		EQU	$-1
	dec	BL
	jnz	short SINGLE_CHECK2
	ret
DISP2	endp

;
; 表示サブルーチンエントリ
;
;
	EVEN
DISP	proc	near
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	DI,BP
	JMOV	BX,_BX_	; BH=MIDMASK  BL=YLEN
	jmp	near ptr dummlabel	; ダミーで遠いところを
	org	$-2
DISP_ADDRESS	dw	?
JUMP_ORIGIN	label	word
	EVEN
DISP	endp


END
