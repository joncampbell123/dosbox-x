; master library
;
; Description:
;	カンマつき数字文字列を作成する
;
; Function/Procedures:
;	int str_comma( char * buf, long value, unsigned buflen ) ;
;
; Parameters:
;	char * buf	書き込み先
;	char * value	数値
;	unsigned buflen	格納先のバイト数(文字数+1)
;
; Returns:
;	1 = 成功
;	0 = 失敗(桁数が足りない)
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	none
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
;	92/11/26 Initial
;	92/12/13 コメントヘッダを書いただけ
;	94/ 6/16 [M0.23] BUGFIX: retでのsp調整値が間違っていた
;	95/ 3/15 [M0.22k] BUGFIX buflenが文字数として処理されてたので1字戻した

	.MODEL SMALL
	include func.inc

	.CODE

	EXTRN ASM_LBDIV:NEAR
	; DX:AX = DX:AX / CL
	; CH    = DX:AX % CL

func STR_COMMA	; str_comma() {
	push	BP
	mov	BP,SP
	_push	DS
	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+4)*2
	value	= (RETSIZE+2)*2
	buflen	= (RETSIZE+1)*2

	_lds	SI,[BP+buf]
	mov	DI,SI			; DI = buf
	mov	BX,[BP+buflen]
	cmp	BX,0
	jle	short OVERFLOW
	lea	SI,[SI+BX-1]		; SI = buf + buflen - 1
	mov	DX,[BP+value+2]
	mov	AX,[BP+value+0]
	or	DX,DX
	jns	short PLUS
	not	DX			; 負数
	not	AX
	add	AX,1
	adc	DX,0
PLUS:
	xor	BX,BX
	mov	[SI],BL			; '\0'を末尾に書き込む
	cmp	SI,DI
	je	short OVERFLOW

VLOOP:
	cmp	BX,3
	jne	short DIGIT
	mov	CH,','
	xor	BX,BX
	jmp	short WRITE
DIGIT:
	mov	CL,10
	call	ASM_LBDIV
	add	CH,'0'
	inc	BX
WRITE:
	dec	SI
	mov	[SI],CH			; 1文字書き込む
	cmp	SI,DI
	je	short OVERFLOW
	mov	CX,DX
	or	CX,AX
	jnz	short VLOOP
	jmp	short TOP
	EVEN

OVERFLOW:
	mov	AX,0		; OVERFLOW; return 0
	jmp	short RETURN
	EVEN

TOP:	test	WORD PTR [BP+value+2],8000h
	jz	short SPACE
	mov	AL,'-'		; 負数なら '-'を書き込む
	dec	SI
	mov	[SI],AL
SPACE:	mov	CX,SI
	sub	CX,DI
	mov	BX,DS
	mov	ES,BX
	mov	AL,' '
	rep	stosb

	mov	AX,1		; SUCCESS; return 1

RETURN:
	pop	DI
	pop	SI
	_pop	DS
	pop	BP
	ret	(3+DATASIZE)*2
endfunc		; }

END
