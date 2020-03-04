; master library - vtext
;
; Description:
;	テキスト画面を消去する
;
; Function/Procedures:
;	void vtext_clear(void) ;
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
;	PC/AT
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
;	のろ/V
;	恋塚
;
; Revision History:
;	93/08/18 Initial
;	93/ 9/13 mov^2 を les に。
;	93/ 9/21 TextVramSize の導入
;	93/11/ 7 VTextState 導入
;	94/01/16 関数名称変更 & 98/AT 両用分離
;		 text_cls -> vtext_clear
;	94/ 4/ 9 Initial: vtclear.asm/master.lib 0.23
;	94/ 4/14 [M0.23] VTextState bit15(テキスト無効ビット)対応
;	94/10/25 [M0.23] VTextState bit14(擬似テキスト)対応
;	95/ 2/16 [M0.22k] 仮想VRAMクリアの属性を00h -> 07hに変更

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramSize : WORD
	EXTRN TextVramWidth : WORD
	EXTRN Machine_State : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_CLEAR		; vtext_clear() {
	CLD
	test	VTextState,4000h		; text emulate mode
	jnz	short EMULATE_CLEAR
	test	VTextState,8000h
	jnz	short EXIT_TEXT_CLS
	xor	CX,CX
	mov	ES,CX		; get video mode

	mov	CX,TextVramSize

	mov	AX,Machine_State
	test	AX,02h		; DOS/V じゃないなら
	je	short TEXT_CLS_NBIOS	; 標準 BIOS にて処理
	test	AX,01h		; DOS/V でも、英語 mode なら
	jne	short TEXT_CLS_NBIOS	; 標準 BIOS にて処理

	mov	AL,ES:[0449h]	; al = [0449h] (video mode)
	cmp	AL,070h		; video mode == 70h なら
	je	short TEXT_CLS_VBIOS	; DOS/V BIOS にて処理
	cmp	AL,003h		; video mode == 03h なら
	je	short TEXT_CLS_VBIOS	; DOS/V BIOS にて処理
TEXT_CLS_NBIOS:
IF 0
	; obsolete
	push	word ptr ES:[0450h] ; save cursor position

	 mov	AH,2
	 xor	DX,DX		; locate(0,0)
	 mov	BH,0
	 int	10h

	 mov	BX,7
	 mov	AX,0920h
	 int	10h		; cls(space で上書き)

	pop	DX		; restore cursor position
	mov	AH,2
	mov	BH,0
	int	10h
ELSE
	; instead
	mov	AX,0600h	; scroll up (clear)
	mov	BH,07h		; attribute
	mov	CX,0		; (0,0)
	mov	DH,ES:[0484h]	; (vtext_height-1)
	mov	DL,byte ptr TextVramWidth
	dec	DL
	int	10h
ENDIF
	jmp	short EXIT_TEXT_CLS

EMULATE_CLEAR:
	push	DI
	les	DI,TextVramAdr
	mov	BX,TextVramSize
ELOOP:
	mov	AX,0720h	; 白 空白
	cmp	ES:[DI],AX
	je	short ESKIP
	mov	ES:[DI],AX
	mov	CX,1
	mov	AH,0ffh
	int	10h
ESKIP:
	add	DI,2
	dec	BX
	jne	short ELOOP
	jmp	short NO_REF
	EVEN

TEXT_CLS_VBIOS:
	push	DI

	les	DI,TextVramAdr
	mov	AX,0720h	; 表示属性 表示文字

	push	CX
	push	DI
	rep	stosw		; 仮想 VRAM 領域クリア
	pop	DI
	pop	CX

	test	VTextState,1
	jnz	short NO_REF

	mov	ah,0ffh		; Refresh Screen
	int	10h

NO_REF:
	pop	DI

EXIT_TEXT_CLS:
	ret
endfunc				; }

END
