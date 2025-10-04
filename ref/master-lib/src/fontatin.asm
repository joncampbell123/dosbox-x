; master library - PC/AT - font
;
; Description:
;	DOS/V フォント読み取り関数の準備
;
; Functions/Procedures:
;	void font_at_init(void);
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	DOS/V
;
; Requiring Resources:
;	CPU: 
;
; Notes:
;	・font読み取り関数(font_AnkFunc,font_KanjiFunc)をインストールします。
;	日本語モードでこの処理を行うことにより、英語の画面モードに切り替えても
;	漢字のフォントが扱えるようになります。
;
; Assembly Language Note:
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
;	94/ 7/27 Initial: atfont2.asm/master.lib 0.23
;	95/ 2/24 [M0.22k] SuperDriversがint 15hで割り込み禁止してるのに対処
;	95/ 3/11 [M0.22k] 英語モードなら半角は英語BIOSから処理するようにした
;			(今までは英語モードだと半角すら見えなかった)
;	95/ 3/11 [M0.22k] すでに実行されている時はスキップするようにした

	.MODEL SMALL
	include func.inc

LANG_US equ 1

	.DATA
	EXTRN font_AnkFunc:DWORD
	EXTRN font_KanjiFunc:DWORD

	.DATA?

ank_font_table dd	?

	.CODE

; in: CX=code, ES:SI=write to
; 8x14dot fontを8x16dotとして処理する専用っす
us_font_read proc far
	push	CX
	push	SI
	push	DI
	push	DS
	mov	DI,SI
	mov	AL,14
	lds	SI,ank_font_table
	mul	CL
	add	SI,AX
	mov	AX,0
	CLD
	stosb			; 0
	mov	CX,14/2
	rep	movsw
	stosb			; 0
	pop	DS
	pop	DI
	pop	SI
	pop	CX
	; mov	AL,0
	ret
us_font_read endp


func FONT_AT_INIT	; font_at_init() {
	push	BP
	pushf

	mov	AX,word ptr font_AnkFunc
	or	AX,word ptr font_AnkFunc+2
	jnz	short ANK_SKIP

	mov	AX,5000h
	mov	BX,0
	mov	DX,0810h
	mov	BP,0
	int	15h
	cmp	AH,0
	je	short set_ank

	; US mode
	mov	AX,1130h
	mov	BH,2			; 8x14dot font
	int	10h
	mov	word ptr ank_font_table,BP
	mov	word ptr ank_font_table+2,ES
	push	CS
	pop	ES
	mov	BX,offset us_font_read

set_ank:
	mov	word ptr font_AnkFunc,BX
	mov	word ptr font_AnkFunc+2,ES
ANK_SKIP:

	mov	AX,word ptr font_KanjiFunc
	or	AX,word ptr font_KanjiFunc+2
	jnz	short KANJI_SKIP

	mov	AX,5000h
	mov	BX,0100h
	mov	DX,1010h
	mov	BP,0
	int	15h
	cmp	AH,0
	jne	short _err
	mov	word ptr font_KanjiFunc,BX
	mov	word ptr font_KanjiFunc+2,ES
KANJI_SKIP:
_err:
	popf
	pop	BP
	ret
endfunc		; }

END
