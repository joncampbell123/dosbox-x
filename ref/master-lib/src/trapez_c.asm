;
.186
.MODEL SMALL
.CODE
	EXTRN	ClipYT_seg:WORD
	EXTRN	ClipYT:WORD

	EXTRN	make_linework:NEAR
	EXTRN	draw_trapezoid:NEAR
	EXTRN	draw_trapezoidx:NEAR

	INCLUDE FUNC.INC

; void _pascal c_make_linework( void * trapez, int xt, int xb, int ylen ) ;
FUNC C_MAKE_LINEWORK
	push	BP
	mov	BP,SP
	mov	BX,[BP+(RETSIZE+4)*2]
	mov	DX,[BP+(RETSIZE+3)*2]
	mov	AX,[BP+(RETSIZE+2)*2]
	mov	CX,[BP+(RETSIZE+1)*2]
	call	make_linework
	pop	BP
	ret	8
ENDFUNC


; void _pascal c_draw_trapezoid( unsigned y, unsigned y2 ) ;
FUNC C_DRAW_TRAPEZOID
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	mov	ES,ClipYT_seg
	mov	SI,[BP+(RETSIZE+2)*2]	; y
	mov	DX,[BP+(RETSIZE+1)*2]	; y2
	sub	DX,SI

	sub	SI,ClipYT
	mov	BX,SI	; multiplies 80
	shl	SI,2
	add	SI,BX
	shl	SI,4
	call	draw_trapezoid

	pop	DI
	pop	SI
	pop	BP
	ret	4
ENDFUNC

; void _pascal c_draw_trapezoidx( unsigned y, unsigned y2 ) ;
FUNC C_DRAW_TRAPEZOIDX
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	mov	ES,ClipYT_seg
	mov	SI,[BP+(RETSIZE+2)*2]	; y
	mov	DX,[BP+(RETSIZE+1)*2]	; y2
	sub	DX,SI

	sub	SI,ClipYT
	mov	BX,SI	; multiplies 80
	shl	SI,2
	add	SI,BX
	shl	SI,4
	call	draw_trapezoidx

	pop	DI
	pop	SI
	pop	BP
	ret	4
ENDFUNC
END
