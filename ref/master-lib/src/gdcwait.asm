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

LEMPTY:	; GDC FIFO����ɂȂ�܂ő҂�
	in	AL,0a0h
	and	AL,4	; empty
	jz	LEMPTY

	mov	AH,8	; timeout

LSTART:	; �`����J�n����܂ő҂�
	in	AL,0a0h
	and	AL,8	; drawing
	jnz	short LEND
	dec	AH
	jnz	LSTART

LEND:	; �`����I������܂ő҂�
	in	AL,0a0h
	and	AL,8	; drawing
	jnz	LEND
	mov	GDCUsed,0
	ret
endfunc

END
