; master library module
;
; Description:
;	ÇQÉoÉCÉgï∂éöÇÃï`âÊ
;
; Functions/Procedures:
;	void vgc_kanji_putc( int x, int y, unsigned c ) ;
;
; Parameters:
;	x,y	ç∂è„í[ÇÃç¿ïW
;	c	ï∂éö
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16Color
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
;	óˆíÀ(óˆíÀè∫ïF)
;
; Revision History:
;	94/ 7/30 Initial: vgckputs.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramWidth:WORD

	.CODE
	EXTRN	FONT_AT_READ:CALLMODEL
FONTHEIGHT equ 10h
FONTSIZE equ 1010h

func VGC_KANJI_PUTC	; vgc_kanji_putc() {
	enter	32,0
	push	SI
	push	DI
	_push	DS

	; à¯êî
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	c	= (RETSIZE+1)*2

	fontbuf = -32

	CLD

	push	[BP+c]
	push	FONTSIZE
	push	SS
	lea	SI,[BP+fontbuf]
	push	SI
	_call	FONT_AT_READ	; font_at_read(ax,FONTSIZE,fontbuf);

	mov	ES,graph_VramSeg

	mov	AX,graph_VramWidth
	imul	word ptr [BP+y]
	mov	DI,[BP+x]
	mov	CX,DI
	and	CL,7
	shr	DI,3
	add	DI,AX		;GVRAM offset address

	mov	DX,FONTHEIGHT

	EVEN
YLOOP:
	lods	word ptr SS:[SI]
	xchg	AH,AL
	mov	BH,AL
	mov	BL,0
	shr	AX,CL
	shr	BX,CL
	test	ES:[DI],AH		; fill latch
	mov	ES:[DI],AH		; write
	test	ES:[DI+1],AL		; fill latch
	mov	ES:[DI+1],AL		; write
	test	ES:[DI+2],BL		; fill latch
	mov	ES:[DI+2],BL		; write

	add	DI,graph_VramWidth	; next line

	dec	DX
	jnz	short YLOOP

RETURN:
	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	3*2
endfunc			; }

END
