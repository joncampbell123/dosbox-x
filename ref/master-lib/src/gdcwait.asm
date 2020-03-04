; 92/7/27
	.MODEL SMALL
	.DATA?

	EXTRN GDCUsed:WORD

	.CODE
	include func.inc

func GDC_WAIT
	cmp	GDCUsed,0
	jne	short LEMPTY
	ret

LEMPTY:	; GDC FIFO‚ª‹ó‚É‚È‚é‚Ü‚Å‘Ò‚Â
	in	AL,0a0h
	and	AL,4	; empty
	jz	LEMPTY

	mov	AH,8	; timeout

LSTART:	; •`‰æ‚ğŠJn‚·‚é‚Ü‚Å‘Ò‚Â
	in	AL,0a0h
	and	AL,8	; drawing
	jnz	short LEND
	dec	AH
	jnz	LSTART

LEND:	; •`‰æ‚ğI—¹‚·‚é‚Ü‚Å‘Ò‚Â
	in	AL,0a0h
	and	AL,8	; drawing
	jnz	LEND
	mov	GDCUsed,0
	ret
endfunc

END
