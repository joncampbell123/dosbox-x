; master library - graphic - wfont - putc - GRCG - PC98V
;
; Description:
;	ÉOÉâÉtÉBÉbÉNâÊñ Ç÷ÇÃà≥èk1ï∂éöï`âÊ
;
; Function/Procedures:
;	void graph_wfont_put(int x, int y, const char * str);
;
; Parameters:
;	x,y	ï`âÊç¿ïW
;	str	ï∂éöÇÃÇ©Ç©ÇÍÇΩÉAÉhÉåÉX
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
;
; Revision History:
;	93/ 7/ 2 Initial: grpwfput.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_RegïœêîëŒâû


	.186
	.MODEL SMALL
	include func.inc
	
	.DATA

	EXTRN	wfont_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN	wfont_Reg:BYTE

	.CODE

func GRAPH_WFONT_PUT	; graph_wfont_put() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	x	= (RETSIZE+2+DATASIZE)*2
	y	= (RETSIZE+1+DATASIZE)*2
	kanji	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	_push	DS
	_lds	SI,[BP+kanji]
	mov	SI,[SI]
	_pop	DS

	; GRCG setting
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	; CG dot access mode
	mov	AL,0bh
	out	68h,AL

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	mov	AX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address

	mov	ES,graph_VramSeg

	mov	AX,SI
	test	AL,0e0h
	jns	short ANKPUT	; 00Å`7f = ANK
	jp	short ANKPUT	; 80Å`9f, e0Å`ff = äøéö
	xchg	AH,AL

KANJIPUT:
	shl	AH,1
	cmp	AL,9fh
	jnb	short SKIP
		cmp	AL,80h
		adc	AX,0fedfh	; (stc)	; -2,-(20h + 1)
SKIP:	sbb	AL,-2
	and	AX,7f7fh
	out	0a1h,AL
	mov	AL,AH
	out	0a3h,AL
	mov	DX,8
	xor	CH,CH

KANJI_LOOP:
	; GRCG setting..
	mov	AL,wfont_Plane1	;RMW mode
	out	7ch,AL

	mov	AL,CH
	or	AL,00100000b	;L/R
	out	0a5h,AL

	in	AL,0a9h
	mov	AH,AL
	mov	AL,CH
	out	0a5h,AL
	in	AL,0a9h

	mov	BH,AL
	mov	BL,0
	shr	AX,CL
	shr	BX,CL
	xchg	AH,AL
	mov	ES:[DI],AX
	mov	ES:[DI+2],BL
	inc	CH

	; GRCG setting..
	mov	AL,wfont_Plane2	;RMW mode
	out	7ch,AL

	mov	AL,CH
	or	AL,00100000b	;L/R
	out	0a5h,AL

	in	AL,0a9h
	mov	AH,AL
	mov	AL,CH
	out	0a5h,AL
	in	AL,0a9h

	mov	BH,AL
	mov	BL,0
	shr	AX,CL
	shr	BX,CL
	xchg	AH,AL
	mov	ES:[DI],AX
	mov	ES:[DI+2],BL

	add	DI,80		;next line

	inc	CH
	dec	DX
	jnz	short KANJI_LOOP
	jmp	short RETURN
	EVEN

ANKPUT:
	mov	AH,0
	mov	SI,AX
	shl	SI,3
	; GRCG setting..
	mov	AL,wfont_Plane1	;RMW mode
	and	AL,wfont_Plane2
	out	7ch,AL

	mov	DX,DS
	mov	DS,wfont_AnkSeg
	mov	CH,8

ANK_LOOP:
	lodsb
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,78
	dec	CH
	jnz	short ANK_LOOP
	mov	DS,DX		;DS restore
	EVEN

RETURN:
	; CG code access
	mov	AL,0ah
	out	68h,AL
	; GRCG off
	mov	AL,0
	out	7ch,AL

	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc			; }

END
