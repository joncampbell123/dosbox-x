; master library - graphic - wfont - putc - VGA
;
; Description:
;	ÉOÉâÉtÉBÉbÉNâÊñ Ç÷ÇÃà≥èkï∂éöóÒï`âÊ
;
; Function/Procedures:
;	void vga4_wfont_puts(int x, int y, int step, const char * str);
;
; Parameters:
;	x,y	ï`âÊç¿ïW
;	step	ï∂éöÇÃëóÇË(16=Ç“Ç¡ÇΩÇË)
;	str	ëSäpÅEîºäpç¨ç›ï∂éöóÒ
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
;	GRCG
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(âúìc  êm)
;	óˆíÀ
;
; Revision History:
;	93/ 7/ 2 Initial: grpwfpts.asm/master.lib 0.20
;	94/ 5/29 Initial: vg4wfpts.asm/master.lib 0.23


	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD
	EXTRN	wfont_Plane1:BYTE,wfont_Plane2:BYTE

	.CODE
	EXTRN	FONT_AT_READ:CALLMODEL

retfunc RETURN
;	mov	DX,SEQ_PORT
;	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
;	out	DX,AX

	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(3+DATASIZE)*2
endfunc

func VGA4_WFONT_PUTS		; vga4_wfont_puts() {
	enter	38,0
	push	SI
	push	DI
	_push	DS

	; à¯êî
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	kanji	= (RETSIZE+1)*2

	; ÉçÅ[ÉJÉãïœêî
	kanjibuf = -32
	wplane1  = -34
	wplane2  = -36
	gwidth   = -38

	mov	ES,graph_VramSeg

	mov	DX,VGA_PORT
	mov	AX,VGA_SET_RESET_REG or (0fh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_OR shl 8)
	out	DX,AX
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX

	mov	AL,SEQ_MAP_MASK_REG
	mov	AH,wfont_Plane1
	not	AH
	and	AH,0fh
	mov	[BP+wplane1],AX
	mov	AH,wfont_Plane2
	not	AH
	and	AH,0fh
	mov	[BP+wplane2],AX

	mov	AX,graph_VramWidth
	mov	[BP+gwidth],AX

	CLD

	mov	CX,[BP+x]
	mul	word ptr [BP+y]
	mov	DI,AX
	_lds	SI,[BP+kanji]

	lodsb
	or	AL,AL
	jz	short RETURN

START:
	mov	BX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	BX,3		;BX=x/8
	add	DI,BX		;GVRAM offset address

	test	AL,0e0h
	jns	short ANKPUTJ	; 00Å`7f = ANK
	jnp	short KANJI_PUT	; 80Å`9f, e0Å`ff = äøéö

ANKPUTJ:
	jmp	ANKPUT

	EVEN
KANJI_PUT:
	mov	AH,AL
	lodsb

	push	SI

	push	ES
	push	CX

	push	AX
	push	01010h
	push	SS
	lea	SI,[BP+kanjibuf]
	push	SI
	call	FONT_AT_READ

	pop	CX
	pop	ES

	mov	BX,8
	EVEN

KANJI_LOOP:
	mov	DX,SEQ_PORT
	mov	AX,[BP+wplane1]
	out	DX,AX

	mov	AX,SS:[SI]
	mov	DL,0
	xchg	AH,AL
	mov	DH,AL
	shr	AX,CL
	shr	DX,CL
	test	ES:[DI],BL
	mov	ES:[DI],AH
	test	ES:[DI+1],BL
	mov	ES:[DI+1],AL
	test	ES:[DI+2],BL
	mov	ES:[DI+2],DL

	mov	DX,SEQ_PORT
	mov	AX,[BP+wplane2]
	out	DX,AX

	mov	AX,SS:[SI+2]
	mov	DL,0
	xchg	AH,AL
	add	SI,4
	mov	DH,AL
	shr	AX,CL
	shr	DX,CL
	test	ES:[DI],BL
	mov	ES:[DI],AH
	test	ES:[DI+1],BL
	mov	ES:[DI+1],AL
	test	ES:[DI+2],BL
	mov	ES:[DI+2],DL

	add	DI,[BP+gwidth]		;next line

	dec	BX
	jnz	KANJI_LOOP

	mov	CH,0
	add	CX,[BP+step]

	pop	SI

LOOPBACK:
	mov	AX,[BP+gwidth]
	shl	AX,3
	sub	DI,AX

	lodsb
	or	AL,AL
	jnz	short RESTART
	jmp	RETURN
	EVEN
RESTART:
	jmp	START
	EVEN

ANKPUT:
	mov	CH,8

	push	DS
	mov	AH,0
	mov	BX,AX
	shl	BX,3

	; êFÇÃê›íË
	mov	DX,SEQ_PORT
	mov	AX,[BP+wplane1]
	or	AX,[BP+wplane2]
	out	DX,AX
	mov	DX,[BP+gwidth]

	mov	DS,wfont_AnkSeg
ANK_LOOP:
	mov	AL,[BX]
	mov	AH,0
	ror	AX,CL
	test	ES:[DI],BL
	mov	ES:[DI],AL
	test	ES:[DI+1],BL
	mov	ES:[DI+1],AH
	add	DI,DX
	inc	BX
	dec	CH
	jnz	short ANK_LOOP

	pop	DS

	mov	AX,[BP+step]
	shr	AX,1		;step / 2
	add	CX,AX		;CH == 0!!

	jmp	short LOOPBACK
endfunc		; }

END
