; master library - GRPHICS - GRCG - GAIJI - PC98V
;
; Description:
;	グラフィック画面へ外字を描画する
;
; Function/Procedures:
;	void graph_gaiji_putc( int x, int y, int ank, int color )
;
; Parameters:
;	x,y	開始左上座標
;	ank	半角文字コード
;	color	色( 0～15 )
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
;	クリッピングは行っていません。
;	外字があらかじめ定義されている必要があります。
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi
;
; Revision History:
;	93/ 7/16 Initial: grpgjput.asm/master.lib 0.20


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD

	.CODE

func GRAPH_GAIJI_PUTC	; graph_gaiji_putc() {
	push	BP
	mov	BP,SP
	push	DI

	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	ank	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	DX,[BP+color]
	mov	BP,[BP+ank]	;BP
	adc	BP,5680h	;from gjwrite.asm
	and	BP,0ff7fh

	; GRCG setting..
	pushf
	mov	AL,0c0h		;RMW mode
	CLI
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

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	ES,graph_VramSeg

	mov	AX,BP
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

	; CG code access
	mov	AL,0ah
	out	68h,AL

	; GRCG off
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	BP
	ret	4*2
endfunc			; }

END
