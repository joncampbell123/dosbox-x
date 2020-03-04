; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	graph_font_put
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(‰œ“c  m)
;	—ö’Ë(—ö’Ëº•F)
;
; Revision History:
;
;$Id: fontput.asm 0.04 93/01/14 23:45:37 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
	
	.DATA

	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func GRAPH_FONT_PUT
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	_push	DS
	_lds	SI,[BP+kanji]
	mov	SI,[SI]
	_pop	DS
	mov	DX,[BP+color]

	; GRCG setting
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
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
	jns	short ANKPUT	; 00`7f = ANK
	jp	short ANKPUT	; 80`9f, e0`ff = Š¿š
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
	mov	DX,16
	xor	CH,CH

KANJI_LOOP:
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
	stosw
	mov	ES:[DI],BL
	add	DI,78		;next line

	inc	CH
	dec	DX
	jnz	short KANJI_LOOP
	jmp	short RETURN
	EVEN

ANKPUT:
	mov	DX,DS		;DS backup
	mov	SI,0
	mov	AH,0
	add	AX,font_AnkSeg
	mov	DS,AX
	mov	CH,16
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
	ret	(3+DATASIZE)*2
endfunc

END
