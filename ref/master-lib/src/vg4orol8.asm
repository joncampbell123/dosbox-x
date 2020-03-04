; master library
;
; Description:
;	VGA 16color, 画面の上下をつないだ透明処理を行わないパターン表示[横8dot単位]
;
; Functions/Procedures:
;	void vga4_over_roll_put_8( int x, int y, int num ) ;
;
; Parameters:
;	x,y	左上端の座標
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
;	横座標は8の倍数に切り捨てられる。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	94/ 8/16 Initial: vg4orol8.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN super_patsize:WORD
	EXTRN super_patdata:WORD
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramWidth:WORD
	EXTRN graph_VramLines:WORD
	EXTRN graph_VramWords:WORD

	.CODE

func VGA4_OVER_ROLL_PUT_8	; vga4_over_roll_put_8() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	CLD
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	AX,[BP+y]
	mov	BP,[BP+x]
	push	AX				; save y

	mov	ES,graph_VramSeg
	mov	DI,graph_VramWidth
	mul	DI
	shr	BP,3		;BP=x/8
	add	BP,AX		;GVRAM offset address

	mov	SI,graph_VramLines
	mov	CX,graph_VramWords
	shl	CX,1
	mov	CS:VRAMSIZE,CX
	mov	CX,super_patsize[BX]		;pattern size (1-8)
	mov	DS,super_patdata[BX]		;BX+2 -> BX

	pop	AX				; restore y
	mov	DX,CX
	xor	DH,DH
	add	DX,AX			; y + ylen - vramlines
	sub	DX,SI
	jg	short SKIP
	mov	DL,0
SKIP:
	mov	CS:YLEN2,DL	; 画面下にはみ出たline数

	mov	AL,CH		; skip mask pattern
	mul	CL
	mov	SI,AX

	sub	CL,DL		; ylen -= ylen2

	mov	BX,DI
	sub	BL,CH

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
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
endfunc			; }

DISP	proc near		; {
	mov	DX,SEQ_PORT
	out	DX,AX

	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
LOOP1:
	mov	CL,AH
	rep	movsb
	add	DI,BX
	dec	DH
	jnz	short LOOP1

	mov	DH,0
	org $-1
YLEN2	db	?
	cmp	DH,0
	je	short DISPEND

	sub	DI,1234h
	org $-2
VRAMSIZE dw	?
	EVEN
LOOP2:
	mov	CL,AH
	rep	movsb
	add	DI,BX
	dec	DH
	jnz	short LOOP2

DISPEND:
	mov	CX,AX
	ret
DISP	endp			; }


END
