; master library - graphic - wfont - putc - GRCG - PC98V
;
; Description:
;	グラフィック画面への圧縮文字列描画
;
; Function/Procedures:
;	void graph_wfont_puts(int x, int y, int step, const char * str);
;
; Parameters:
;	x,y	描画座標
;	step	文字の送り(16=ぴったり)
;	str	全角・半角混在文字列
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
;	93/ 7/ 2 Initial: grpwfpts.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_Reg変数対応


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN	wfont_Reg:BYTE

	.CODE

retfunc RETURN
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
endfunc

func GRAPH_WFONT_PUTS		; graph_wfont_puts() {
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

	; GRCG setting
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	_lds	SI,[BP+kanji]
	mov	BP,[BP+step]

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	lodsb
	or	AL,AL
	jz	short RETURN

START:
	mov	BX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	BX,3		;BX=x/8
	add	DI,BX		;GVRAM offset address

	test	AL,0e0h
	jns	short ANKPUTJ	; 00～7f = ANK
	jnp	short KANJI_PUT	; 80～9f, e0～ff = 漢字

ANKPUTJ:
	jmp	ANKPUT

	EVEN
KANJI_PUT:
	mov	AH,AL

	; CG dot access		;←ここにあるのは、漢字を含まない文字列のときは
	mov	AL,0bh		; テキスト画面が乱れないようにするため～
	out	68h,AL

	lodsb
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
	INC	CH

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
	jnz	KANJI_LOOP

	xor	CH,CH
	add	CX,BP

LOOPBACK:
	sub	DI,80*8

	lodsb
	or	AL,AL
	jnz	short RESTART
	jmp	RETURN
	EVEN
RESTART:
	jmp	START
	EVEN

ANKPUT:
	mov	CH,8

	mov	DX,DS
	mov	AH,0
	mov	BX,AX
	shl	BX,3

	; 色の設定
	mov	AL,wfont_Plane1	;RMW mode
	and	AL,wfont_Plane2
	out	7ch,AL

	mov	DS,wfont_AnkSeg
ANK_LOOP:
	mov	AL,[BX]
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,78
	inc	BX
	dec	CH
	jnz	short ANK_LOOP

	mov	DS,DX		;DS restore

	mov	AX,BP
	shr	AX,1		;step / 2
	add	CX,AX		;CH == 0!!

	jmp	short LOOPBACK
endfunc		; }

END
