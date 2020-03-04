; master library - PC98
;
; Description:
;	フォントパターンの登録
;
; Function/Procedures:
;	void font_write( unsigned short chr, const void * pattern ) ;
;
; Parameters:
;	unsigned chr		文字コード( JISまたはシフトJIS )
;	void * pattern 		左半分が上から16bytes,続けて右半分、と
;				並ぶパターン
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
;	文字コードが外字でない場合、なにも起こりません。
;
; Assembly Language Note:
;	メモリ不足などの場合、cy=1が返ります。
;	成功ならばcy=0です。
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
;	93/10/16 [M0.21] 半角文字の登録がうごかなかった(^^; bugfix
;	94/ 1/24 [M0.22] font_AnkSizeを見る
;	94/ 8/ 5 [M0.23] font_AnkParaも設定
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

	.DATA
	EXTRN font_AnkSeg:WORD
	EXTRN font_AnkSize:WORD
	EXTRN font_AnkPara:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
; IN:
;	AH = 20h:L, 00h:R
;	DS:SI = read pointer
SETFONT proc near
	mov	CX,16
GLOOP:	mov	AL,AH		; キャラクタＲＯＭへ
	out	0a5h,AL		; 漢字フォントを書き込む
	lodsb
	out	0a9h,AL
	inc	AH
	loop	short GLOOP
	ret
SETFONT endp

func FONT_WRITE		; font_write() {
	mov	DX,SP
	CLI
	add	SP,RETSIZE*2
	pop	BX	; pattern
	_pop	CX	; FP_SEG(pattern)
	pop	AX	; chr
	mov	SP,DX
	STI

	push	SI

	test	AH,AH
	jnz	short KANJI

	; 半角ANKは、メモリ内データから読むのみ
	mov	DX,font_AnkSeg

	cmp	font_AnkSize,0110h	; 8x16dot
	jne	short REALLOC
	test	DX,DX
	jnz	short CONT
	jmp	short ALLOC2
REALLOC:
	test	DX,DX
	jz	short ALLOC2
	push	AX
	push	DX
	_call	HMEM_FREE
	pop	AX
	mov	font_AnkSeg,0
ALLOC2:
	; font_AnkSegが設定されてないとき
	mov	mem_AllocID,MEMID_font
	push	AX
	mov	AX,256		; 256char * 16 bytes
	push	AX
	_call	HMEM_ALLOC
	jc	short DAME
	mov	font_AnkSeg,AX
	mov	font_AnkSize,0110h	; 8x16dot
	mov	font_AnkPara,1
	mov	DX,AX
DAME:
	pop	AX
	jc	short EXIT
CONT:
	push	DI
	xor	DI,DI
	mov	SI,BX
	add	AX,DX
	mov	ES,AX
	_mov	BX,DS
	_mov	DS,CX
	mov	CX,8
	rep movsw		; write to *bufadr
	_mov	DS,BX
	pop	DI
	CLC
	jmp	short EXIT
	EVEN

KANJI:
	_push	DS
	_mov	DS,CX

	mov	SI,BX

	; 日本語の文字
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
	call	SETFONT
	mov	AH,00h
	call	SETFONT

	mov	AL,0ah		; ＣＧコードアクセスにするん
	out	68h,AL

	_pop	DS
	CLC
EXIT:
	pop	SI
	ret	(DATASIZE+1)*2
endfunc			; }

END
