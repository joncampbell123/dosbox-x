; master library - PC98
;
; Description:
;	フォントパターンの読みだし
;
; Function/Procedures:
;	void font_read( unsigned short chr, void * pattern ) ;
;
; Parameters:
;	unsigned chr		文字コード( JISまたはシフトJIS )
;	void * pattern 		左半分が上から16bytes,続けて右半分、と
;				並ぶパターン
;				ANKの場合は、左半分だけ格納される。
;				　ただし読み込み元は、CGではなくあらかじめ
;				メモリ上に作成されたデータである。
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
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial
;	93/ 1/27 bugfix

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN font_AnkSeg:WORD

	.CODE

; IN:
;	AH = ( bit 0..3 = line,   bit 5 = L/R )
;	ES:DI = write pointer
GETFONT proc near
	mov	CX,16
GLOOP:	mov	AL,AH		; キャラクタＲＯＭから
	out	0a5h,AL		; 漢字フォントを読み出す処理
	jmp	short $+2
	in	AL,0a9h
	stosb
	inc	AH
	loop	short GLOOP
	ret
GETFONT endp


func FONT_READ
	mov	DX,SP
	CLI
	add	SP,RETSIZE*2
	pop	BX	; pattern
	_pop	ES	; FP_SEG(pattern)
	pop	AX	; chr
	mov	SP,DX
	STI

	push	DI

	mov	DI,BX

  s_ 	<mov	CX,DS>
  s_	<mov	ES,CX>

	or	AH,AH
	jnz	short READ_JAP
		; 半角ANKは、メモリ内データから読むのみ
		mov	DX,SI
		xor	SI,SI
		xor	AH,AH
		add	AX,font_AnkSeg
		mov	BX,DS
		mov	DS,AX
		mov	CX,8
		rep movsw		; write to *bufadr
		mov	DS,BX
		mov	SI,DX
	jmp	short EXIT

	; 日本語の文字
READ_JAP:
	test	AH,80h
	jz	short JIS
		; SHIFT-JIS to JIS
		shl	AH,1
		cmp	AL,9fh
		jnb	short SKIP
			cmp	AL,80h
			adc	AX,0fedfh
SKIP:		sbb	AX,0dffeh		; -(20h+1),-2
		and	AX,07f7fh

JIS:
	push	AX
	mov	AL,0bh		; ＣＧドットアクセスにするん
	out	68h,AL
	pop	AX

	sub	AH,20h
	out	0a1h,AL		; JISコード下位
	xchg	AH,AL
	jmp	short $+2
	jmp	short $+2
	out	0a3h,AL		; JISコード上位
	mov	AH,20h
	call	GETFONT
	mov	AH,00h
	call	GETFONT

	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL
EXIT:
	pop	DI
	ret	(DATASIZE+1)*2
endfunc

END
