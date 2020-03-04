; master library - PC98
;
; Description:
;	テキスト画面の部分保存/復元
;
; Function/Procedures:
;	保存
;	void text_get( int x1,int y1, int x2,int y2, void far *buf ) ;
;
;	復元
;	void text_put( int x1,int y1, int x2,int y2, const void far *buf ) ;
;
; Parameters:
;	int x1,x2 : 0～79
;	int y1,y2 : 0～
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal/Hireso
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	text_getはくりっぴんぐしてないよ
;	text_putは横だけクリッピングしてるよ
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 2/ 5 Initial
;
	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramSeg : WORD

	.CODE

func TEXT_GET	; {
	push	BP
	mov	BP,SP

	; 引数
	x1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	x2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	mov	SI,[BP+x1]	; もし x1 > x2 なら、
	mov	DX,[BP+x2]	;	x1 <-> x2
	cmp	SI,DX		;
	jle	short GET_SKIP1	;
	xchg	SI,DX		;
GET_SKIP1:			; SI = x1
	sub	DX,SI		; DX = x2 - SI + 1
	inc	DX		;

	mov	AX,[BP+y1]	; AX = y1
	mov	BX,[BP+y2]
	cmp	BX,AX
	jg	short GET_SKIP2
	xchg	AX,BX
GET_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	CX,AX		; SI += y1 * 80
	shl	AX,2		;
	add	AX,CX		;
	shl	AX,4		;
	add	SI,AX		;

	shl	SI,1		; SI <<= 1

	mov	DS,TextVramSeg
	les	DI,[BP+buf]
	mov	AX,DX
	shl	AX,1

GET_LOOP:	; 縦のループ
	mov	CX,DX
	rep movsw	; 文字
	sub	SI,AX
	add	SI,2000h
	mov	CX,DX
	rep movsw	; 属性
	mov	CX,DX
	sub	SI,AX

	add	SI,160-2000h	; 文字の次の行
	dec	BX
	jnz	short GET_LOOP

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc		; }

func TEXT_PUT ; {
	push	BP
	mov	BP,SP

	; 引数
	x1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	x2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	mov	ES,TextVramSeg
	lds	SI,[BP+buf]

	mov	DI,[BP+x1]	; もし x1 > x2 なら、
	mov	DX,[BP+x2]	;	x1 <-> x2
	cmp	DI,DX		;
	jle	short PUT_SKIP1	;
	xchg	DI,DX		;
PUT_SKIP1:
	sub	DX,DI		; DI = x1
	inc	DX		; DX = x2 - DI + 1

	mov	CX,0		; CXが引く流さになるよーん
	mov	BX,DX		; BX = 横ドット数
	mov	AX,DI		;
	cwd
	and	AX,DX
	mov	CX,AX		; CX = 左にはみでたドット数(負数)
	sub	DI,CX		; DI = ひだりにはみでていれば 0
	sub	SI,CX		; SI += ひだりにはみ出たバイト数*2
	sub	SI,CX		; 
	lea	AX,[BX+DI]
	sub	AX,80
	cmc
	sbb	DX,DX
	and	AX,DX
	sub	CX,AX
	add	BX,CX
	js	short CLIPOUT
	mov	DX,BX
	shl	CX,1
	push	CX		; スキップ分 -> stack

	mov	AX,[BP+y1]	;
	mov	BX,[BP+y2]	;
	cmp	AX,BX		;
	jle	short PUT_SKIP2	; もし y1 > y2 なら、
	xchg	AX,BX		;	y1 <-> y2
PUT_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;
	mov	CX,AX		; DI += y1 * 80
	shl	AX,2		;
	add	AX,CX		;
	shl	AX,4		;
	add	DI,AX		;

	shl	DI,1		; DI <<= 1

	mov	AX,DX
	shl	AX,1
	pop	BP		; stack -> ソースのスキップ量(負数)

PUT_LOOP:
	mov	CX,DX
	rep movsw		; 文字
	sub	SI,BP
	sub	DI,AX
	add	DI,2000h
	mov	CX,DX
	rep movsw		; 属性
	sub	SI,BP
	sub	DI,AX
	add	DI,160-2000h
	dec	BX
	jne	short PUT_LOOP
CLIPOUT:
	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc	; }

END
