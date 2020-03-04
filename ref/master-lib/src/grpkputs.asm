; superimpose & master library module
;
; Description:
;	‚QƒoƒCƒg•¶š‚Ì‚İ(‚Ğ‚ç‚ª‚ÈAŠ¿š‚È‚Ç)‚©‚ç‚È‚é•¶š—ñ‚Ì•`‰æ
;
; Functions/Procedures:
;	void graph_kanji_puts( int x, int y, int step, char * kanji, int color ) ;
;
; Parameters:
;	x,y	¶ã’[‚ÌÀ•W
;	step	—×‰ï‚¤•¶š‚Ì¶’[“¯m‚ÌŠÔŠu(dot”)
;	kanji	•¶š—ñ
;	color	F(0`15)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
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
;$Id: kanjputs.asm 0.01 92/06/06 14:23:34 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD

	.CODE
func GRAPH_KANJI_PUTS
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ˆø”
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+step]
	_lds	SI,[BP+kanji]
	mov	DX,[BP+color]
	mov	BP,BX

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	; GRCG setting..
	mov	AL,0c0h		;RMW mode
	pushf
	cli
	out	7ch,AL
	popf
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

	; CG dot access
	mov	AL,0bh
	out	68h,AL

	lodsw
	or	AL,AL
	jz	short RETURN
START:
	mov	BX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	xchg	AL,AH

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
	EVEN
PUT_LOOP:
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
	xchg	AL,AH
	stosw
	mov	ES:[DI],BL
	add	DI,78		;next line
	inc	CH
	dec	DX
	jnz	short PUT_LOOP
	sub	DI,80 * 16
	xor	CH,CH
	add	CX,BP

	lodsw
	or	AL,AL
	jnz	short START

RETURN:
	; CG code access
	mov	AL,0ah
	out	68h,AL
	; GRCG off
	xor	AL,AL
	out	7ch,AL		;grcg stop

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(4+DATASIZE)*2
endfunc

END
