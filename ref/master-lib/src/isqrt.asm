;
; Description:
;	四捨五入つき平方根
;	距離
;
; Function:
;	int pascal isqrt( long x ) ;
;	int pascal ihypot( int x, int y ) ;
;
; Parameters:
;	long x	... 平方根を求めたい数
;			0 <= x < 32767.5↑2
;			( 0 <= x <= (1073709056 = 3FFF8000h) )
;			これ以外ならばオーバーフロー
;			(a↑bはaのb乗の意味)
;
;	int x,y ... 距離を得たい座標
;
; Returns:
;	int .......... 0 ～ 32767
;			x が 0 またはオーバーフローならば 0
;
; Requiring Resources:
;	CPU: i8086
;
; Author:
;	恋塚昭彦
;
; History:
;	92/ 7/31 Initial
;	92/ 8/ 1

	.MODEL SMALL
	.CODE
	include func.inc

; int ihypot( int x, int y ) ;
func IHYPOT
	push	BP
	mov	BP,SP

	y = (RETSIZE+2)*2
	x = (RETSIZE+1)*2

	mov	AX,[BP+x]
	imul	AX
	mov	BX,AX
	mov	CX,DX
	mov	AX,[BP+y]
	imul	AX
	add	BX,AX
	adc	CX,DX
	jmp	short ROOT
endfunc


; int pascal isqrt( long x ) ;
;
; 実装:ニュートン法
;
func ISQRT
	push	BP
	mov	BP,SP

	hi = (RETSIZE+2)*2
	lo = (RETSIZE+1)*2

	mov	BX,[BP+lo]
	mov	CX,[BP+hi]
ROOT:
	xor	AX,AX		; return code = 0

	mov	DX,BX
	or	DX,CX
	jz	short RETURN	; x = 0 なら return 0
	cmp	CX,03FFFh	; 0～3FFF8000hでなければ OverFlow! return 0
	ja	short RETURN
	jne	short GO
	cmp	BX,08000h
	ja	short RETURN
GO:
	push	SI

	shl	BX,1		; CX:BX = x
	rcl	CX,1
	shl	BX,1
	rcl	CX,1		; x *= 4 ;
	or	CX,CX
if 0
	mov	SI,0FFFFh
	jnz	short LLOOP
	mov	SI,8		; SI: y, BP: lasty
else
	jz	short BXLOOPS

	mov	SI,1	; 8000h rol 1
	mov	AX,3	; 0c000h rol 2
CXLOOP:
	ror	SI,1
	ror	AX,1
	ror	AX,1
	test	CX,AX
	jz	short CXLOOP
	or	AX,AX
	jns	short LLOOP
	mov	SI,0FFFFh	; 最上位bitが立っている場合は overflowを
	jmp	short LLOOP	; 防ぐため特例として 0FFFFhを初期値にする

BXLOOPS:
	mov	SI,100h	; 80h rol 1
	mov	AX,3	; 0c000h rol 2
BXLOOP:
	ror	SI,1
	ror	AX,1
	ror	AX,1
	test	BX,AX
	jz	short BXLOOP
	; jmp short LLOOP
endif

LLOOP:
	mov	BP,SI		; lasty = y
	mov	AX,BX
	mov	DX,CX
	div	SI
	add	SI,AX
	rcr	SI,1		; y = ((unsigned long)x / y + y)/ 2 ;
	cmp	SI,AX
	je	short LEXIT
	cmp	SI,BP
	jne	short LLOOP	; if ( y != lasty ) goto LLOOP ;
LEXIT:
	xor	AX,AX
	shr	SI,1
	adc	AX,SI		; lasty = y & 1 ; return y / 2 + lasty ;

	pop	SI
RETURN:
	pop	BP
	ret	4
endfunc

END
