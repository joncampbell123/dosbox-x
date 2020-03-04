; master library - graphic - wfont - kanji - puts - GRCG - PC98V
;
; Description:
;	グラフィック画面への圧縮全角文字列描画
;
; Function/Procedures:
;	void graph_wkanji_puts(int x, int y, int step, const char * str);
;
; Parameters:
;	x,y    左上座標
;	step   文字間隔(16でぴったり連続します)
;	str    文字列の先頭アドレス
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
;	Kazumi(奥田  仁)
;
; Revision History:
;	93/ 7/ 2 Initial:grpwkpts.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_Reg変数対応


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN wfont_Reg:BYTE

	.CODE

func GRAPH_WKANJI_PUTS	; graph_wkanji_puts() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	kanji	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+step]
	_lds	SI,[BP+kanji]
	mov	BP,BX

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	; GRCG setting..
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	; CG dot access
	mov	AL,0bh
	out	68h,AL

	lodsw
	or	AL,AL
	jnz	short START
	jmp	RETURN
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
	mov	DX,8
	xor	CH,CH
	EVEN
PUT_LOOP:
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
	xchg	AL,AH
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
	xchg	AL,AH
	mov	ES:[DI],AX
	mov	ES:[DI+2],BL

	add	DI,80		;next line
	inc	CH
	dec	DX
	jnz	short PUT_LOOP
	sub	DI,80 * 8
	xor	CH,CH
	add	CX,BP

	lodsw
	or	AL,AL
	jz	short RETURN
	jmp	START

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
	ret	(3+DATASIZE)*2
endfunc			; }

END
