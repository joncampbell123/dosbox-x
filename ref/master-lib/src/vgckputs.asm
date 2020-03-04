; master library module
;
; Description:
;	２バイト文字のみ(ひらがな、漢字など)からなる文字列の描画
;
; Functions/Procedures:
;	void vgc_kanji_puts( int x, int y, int step, char * kanji ) ;
;
; Parameters:
;	x,y	左上端の座標
;	step	隣会う文字の左端同士の間隔(dot数)
;	kanji	文字列
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
;	恋塚(恋塚昭彦)
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

func VGC_KANJI_PUTS	; vgc_kanji_puts() {
	enter	38,0
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	kanji	= (RETSIZE+1)*2

	fontbuf = -32
	vwidth  = -34
	gseg    = -36
	yadr    = -38

	CLD
	_mov	AX,graph_VramSeg
	_mov	[BP+gseg],AX
	mov	AX,graph_VramWidth
	_mov	[BP+vwidth],AX
	imul	word ptr [BP+y]
	mov	[BP+yadr],AX
	_lds	SI,[BP+kanji]

	lodsw
	or	AL,AL
	jz	short RETURN

STRLOOP:
	xchg	AH,AL
	;漢字フォント読み取り

	push	SI

	push	AX
	push	FONTSIZE
	push	SS
	lea	SI,[BP+fontbuf]
	push	SI
	_call	FONT_AT_READ	; font_at_read(ax,FONTSIZE,fontbuf);

	s_mov	ES,graph_VramSeg
	_mov	ES,[BP+gseg]

	mov	DI,[BP+x]
	mov	CX,DI
	and	CL,7
	shr	DI,3
	add	DI,[BP+yadr]	;GVRAM offset address

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

	; next line
    l_ <add	DI,[BP+vwidth]>
    s_ <add	DI,graph_VramWidth>

	dec	DX
	jnz	short YLOOP

	pop	SI

	mov	AX,[BP+step]
	add	[BP+x],AX

	lodsw
	or	AL,AL
	jnz	short STRLOOP

RETURN:
	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(3+DATASIZE)*2
endfunc			; }

END
