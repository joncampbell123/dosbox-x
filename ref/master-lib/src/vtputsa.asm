; master library - vtext
;
; Description:
;	テキスト画面への文字列の書き込み
;	幅指定なし・属性あり
;
; Function/Procedures:
;	void vtext_putsa( unsigned x, unsigned y, char *strp, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 255 )
;	unsigned y	上端の座標 ( 0 ～ 255 )
;	char * strp	文字列の先頭アドレス
;	unsigned atrb	文字属性
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
;	恋塚
;
; Revision History:
;	93/10/06 Initial
;	93/10/08 puts_at() --> vtext_puts()
;	93/11/07 VTextState 導入
;	94/ 4/ 9 Initial: vtputs.asm/master.lib 0.23
;	94/ 5/20 optimize, bugfix large data model
;	94/ 5/20 Initial: vtputsa.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN Machine_State : WORD
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramWidth : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_PUTSA	; vtext_putsa() {
	mov	ax,Machine_State
	test	ax,0010h	; PC/AT じゃないなら
	je	short EXIT

	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	strp	= (RETSIZE+2)*2
	atrb	= (RETSIZE+1)*2

	CLD

	s_push	DS
	s_pop	ES
	_les	DI,[bp+strp]	; ES:DI,SI = string address
	mov	SI,DI		;
	mov	CX,-1
	mov	AL,0
	repne	scasb
	not	CX
	dec	CX	; CX = len(str)
	jz	short EXIT2

	mov	DX,CX		; DX = copy of length of string
	_mov	BX,ES		; BX = segment address of string

	mov	AX,TextVramWidth
	mul	byte ptr [BP+y]	; row
	add	AX,[BP+x]	; column
	add	AX,AX
	les	DI,TextVramAdr
	add	DI,AX

	_push	DS
	_mov	DS,BX
	mov	AH,[BP+atrb]

LOOP_STR:
	lodsb
	stosw
	loop	short LOOP_STR

	_pop	DS

	test	byte ptr VTextState,1
	jne	short NO_REF

	mov	CX,DX
	shl	DX,1
	sub	DI,DX
	mov	AH,0ffh		; Reflesh Screen
	int	10h

NO_REF:
EXIT2:
	pop	DI
	pop	SI
	pop	BP
EXIT:
	ret	(3+datasize)*2
endfunc			; }

END
