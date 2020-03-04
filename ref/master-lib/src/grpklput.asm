; superimpose & master library module
;
; Description:
;	グラフィック画面に漢字を４倍角で１文字表示する
;
; Functions/Procedures:
;	void graph_kanji_large_put(int x, int y, const char *str, int color) ;
;
; Parameters:
;	x,y	描画左上座標
;	str	文字列の先頭アドレス(最初の1文字だけ描画します)
;	color	色(0～15)
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
;	漢字半角は未対応
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;$Id: kanjlrge.asm 0.01 92/06/06 14:20:33 Kazumi Rel $
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 8/ 3 [M0.20] bugfix(LARGE_BYTEが fixup overflow)
;	93/11/22 [M0.21] bugfix, return sizeを2倍してなかった。ありゃあ
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD

	.CODE
	EXTRN LARGE_BYTE:BYTE

func GRAPH_KANJI_LARGE_PUT
	push	BP
	mov	BP,SP
	push	DI

	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	DX,[BP+color]
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

	; GRCG dot access
	mov	AL,0bh
	out	68h,AL

	mov	CX,[BP+x]
	mov	DI,[BP+y]

 	_les	BX,[BP+kanji]
 s_	<mov	AX,[BX]>
 l_	<mov	AX,ES:[BX]>

	xchg	AH,AL

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

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	ES,graph_VramSeg
	mov	DH,16
	xor	CH,CH

PUT_LOOP:
	mov	AL,CH
	or	AL,00100000b	;L/R
	out	0a5h,AL
	in	AL,0a9h
	mov	DL,AL
	shr	AL,4
	xor	AH,AH
	mov	BX,offset _TEXT:LARGE_BYTE
	xlat	byte ptr CS:[BX]
	ror	AX,CL
	mov	ES:[DI],AX
	mov	ES:[DI+80],AX
	inc	DI
	mov	AL,DL
	and	AX,0fh
	mov	BX,offset _TEXT:LARGE_BYTE
	xlat	byte ptr CS:[BX]
	ror	AX,CL
	mov	ES:[DI],AX
	mov	ES:[DI+80],AX
	inc	DI
	mov	AL,CH
	out	0a5h,AL
	in	AL,0a9h
	mov	DL,AL
	shr	AL,4
	xor	AH,AH
	mov	BX,offset _TEXT:LARGE_BYTE
	xlat	byte ptr CS:[BX]
	ror	AX,CL
	mov	ES:[DI],AX
	mov	ES:[DI+80],AX
	inc	DI
	mov	AL,DL
	and	AX,0fh
	mov	BX,offset _TEXT:LARGE_BYTE
	xlat	byte ptr CS:[BX]
	ror	AX,CL
	mov	ES:[DI],AX
	mov	ES:[DI+80],AX
	add	DI,77+80	;next line
	inc	CH
	dec	DH
	jnz	short PUT_LOOP

	; CG code access
	mov	AL,0ah
	out	68h,AL

	; GRCG off
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
