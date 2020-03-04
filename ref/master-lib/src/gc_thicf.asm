PAGE 98,120
; graphics - grcg - thick_line - PC98V
;
; Function:
;	void far _pascal grcg_thick_line( int x1, int y1, int x2, int y2,
;					unsigned wid, unsigned hei ) ;
;
; Description:
;	四角いペンによる直線
;
; Parameters:
;	int x1,y2	第１点(ペンの左上)
;	int x2,y2	第２点(ペンの左上)
;	unsigned wid	ペンの横の太さ
;	unsigned hei	ペンの縦の太さ
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;	このままで全メモリモデルに対応。
;
; Notes:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリッピングを行っています。
;
; 関連関数:
;	grc_setclip()
;	grcg_polygon_c()
;	grcg_line()
;	grcg_boxfill()
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/6/11 Initial
;	92/6/12 あ～あ。/0があ(こればっか)。直した。ぐすし。
;		さらに、単純な交換ミス。ぐすしぐすし。
;	92/6/16 TASM対応


	.186
	.MODEL SMALL
	.CODE
	include func.inc

	EXTRN	GRCG_POLYGON_C:CALLMODEL
	EXTRN	GRCG_LINE:CALLMODEL
	EXTRN	GRCG_BOXFILL:CALLMODEL

	; parameters
	x1  = (RETSIZE+6)*2
	y1  = (RETSIZE+5)*2
	x2  = (RETSIZE+4)*2
	y2  = (RETSIZE+3)*2
	wid = (RETSIZE+2)*2
	hei = (RETSIZE+1)*2

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	12
	EVEN
	endm

retfunc DUMMY
	; 水平または垂直なので、長方形に委譲
BOXFILL:
	cmp	SI,CX
	jge	short S400
	xchg	SI,CX
S400:
	cmp	DI,BX
	jge	short S500
	xchg	BX,DI
S500:
	push	CX
	push	BX
	add	SI,[BP+wid]
	push	SI
	add	DI,[BP+hei]
	push	DI
	call	GRCG_BOXFILL
RETURN:
	MRETURN
endfunc

; void far _pascal grcg_thick_line( int x1, int y1, int x2, int y2,
;				unsigned wid, unsigned hei ) ;

func GRCG_THICK_LINE
	enter	24,0
	push	SI
	push	DI

	; local variables
	pts = -24

	mov	CX,[BP+x1]
	mov	BX,[BP+y1]
	mov	SI,[BP+x2]
	mov	DI,[BP+y2]

	mov	AX,[BP+hei]	; AX = hei
	mov	DX,AX
	or	DX,[BP+wid]
	jne	short S100

	; 太さが縦横ともに 0 なので、直線へ委譲
	push	CX
	push	BX
	push	SI
LINE:
	push	DI
	call	GRCG_LINE
	jmp	RETURN
	EVEN

S100:
	cmp	SI,CX
	je	short BOXFILL
	cmp	DI,BX
	je	short BOXFILL

	or	AX,AX		; hei
	jnz	short POLYGON

	; 縦の太さが 0 なので、直線にするかどうか判定する
	sub	DI,BX	; y2 -= y1
	mov	AX,SI	; AX = x2 - x1
	sub	AX,CX
	cwd
	idiv	DI	; AX /= y2
	cwd
	xor	AX,DX
	sub	AX,DX	; AX = abs(AX)
	add	DI,BX	; y2 += y1

	cmp	AX,[BP+wid]
	jle	short POLYGON

	; 横に太いが、直線で代用する
	cmp	SI,CX
	jge	short S200
	xchg	CX,SI
	xchg	BX,DI
S200:
	push	CX
	push	SI
	mov	AX,[BP+wid]
	add	AX,DI
	push	AX
	jmp	SHORT LINE
	EVEN

POLYGON:
	cmp	DI,BX
	jge	short S300
	xchg	CX,SI
	xchg	BX,DI
S300:
	mov	[BP+pts+0],CX	; p0 = (x1,y1)
	mov	[BP+pts+2],BX

	cmp	SI,CX
	jle	short POLY2

	mov	AX,[BP+wid]
	add	CX,AX		; p1 = (x1+wid,y1)
	mov	[BP+pts+4],CX
	mov	[BP+pts+6],BX

	sub	CX,AX
	add	AX,SI
	mov	[BP+pts+8],AX	; p2 = (x2+wid,y2)
	mov	[BP+pts+10],DI

	mov	DX,[BP+hei]
	add	DI,DX
	mov	[BP+pts+12],AX	; p3 = (x2+wid,y2+hei)
	mov	[BP+pts+14],DI

	mov	[BP+pts+16],SI	; p4 = (x2,y2+hei)
	mov	[BP+pts+18],DI

	mov	[BP+pts+20],CX	; p5 = (x1,y1+hei)
	add	BX,DX
	mov	[BP+pts+22],BX
	jmp	SHORT GO_POLYGON
	EVEN

POLY2:
	mov	AX,[BP+wid]
	add	CX,AX		; p1 = (x1+wid,y1)
	mov	[BP+pts+4],CX
	mov	[BP+pts+6],BX

	mov	DX,[BP+hei]
	add	BX,DX
	mov	[BP+pts+8],CX	; p2 = (x1+wid,y1+hei)
	mov	[BP+pts+10],BX

	add	AX,SI		; p3 = (x2+wid,y2+hei)
	add	DX,DI
	mov	[BP+pts+12],AX
	mov	[BP+pts+14],DX

	mov	[BP+pts+16],SI	; p4 = (x2,y2+hei)
	mov	[BP+pts+18],DX

	mov	[BP+pts+20],SI	; p5 = (x2,y2)
	mov	[BP+pts+22],DI

GO_POLYGON:
IF datasize EQ 2
	push	SS
ENDIF
	lea	AX,[BP+pts]
	push	AX
	push	6
	call	GRCG_POLYGON_C
	MRETURN
endfunc

END
