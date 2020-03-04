; superimpose & master library module
;
; Description:
;	グラフィック画面への文字列の描画
;
; Functions/Procedures:
;	void graph_font_puts(int x, int y, int step, const char MASTER_PTR * str, int color) ;
;
; Parameters:
;	x,y    最初の文字の左上座標
;	step   全角文字単位での 1文字の進み量(16で隙間なし)。
;	       半角は半分(小数点以下切り捨て)で使用される。
;	str    文字列
;	color  色(0..15)
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
;
; Notes:
;	
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
;
;$Id: fontputs.asm 0.03 92/06/06 14:30:22 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	94/ 1/ 5 [M0.22] BUGFIX (large, pascal)
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

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
	ret	(4+DATASIZE)*2
endfunc

func GRAPH_FONT_PUTS		; {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	CLD
	mov	ES,graph_VramSeg

	; GRCG setting
	mov	DX,[BP+color]
	mov	AL,0c0h		;RMW mode
	pushf
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

    l_ <mov	AX,font_AnkSeg>
    l_ <mov	CS:FONTSEG,AX>


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
	jns	short ANKPUT	; 00～7f = ANK
	jp	short ANKPUT	; 80～9f, e0～ff = 漢字

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
	mov	DX,16
	xor	CH,CH
	EVEN

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
	jnz	KANJI_LOOP

	xor	CH,CH
	add	CX,BP

LOOPBACK:
	sub	DI,80*16

	lodsb
	or	AL,AL
	jnz	short START
	jmp	RETURN
	EVEN

ANKPUT:
	mov	CH,16
	mov	DX,DS		;DS backup
	xor	BX,BX
	mov	AH,0
IF DATASIZE EQ 1
	add	AX,font_AnkSeg
ELSE
	add	AX,1111h	; dummy
	org $-2
FONTSEG dw ?
ENDIF
	mov	DS,AX

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
