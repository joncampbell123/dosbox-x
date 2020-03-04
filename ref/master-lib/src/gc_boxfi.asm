PAGE 98,120
; graphics - grcg - boxfill - PC98V
;
; Function:
;	void far _pascal grcg_boxfill( int x1, int y1, int x2, int y2 ) ;
;
; Description:
;	箱塗り(青プレーンのみ)
;
; Parameters:
;	int x1,y2	第１点
;	int x2,y2	第２点
;
; Binding Target:
;	Microsoft-C / Turbo-C
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
;	for ALL memory model(SMALL以外にするときは .MODELを書き換えて下さい)
;
; Notes:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリッピングを行っています。
;
; 関連関数:
;	grc_setclip()
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/6/10 Initial
;	92/6/10 些細な(害の無い)バグ取り
;	92/6/16 TASM対応

	.186
	.MODEL SMALL

	.DATA

	; grc_setclip()関連
	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	; エッジパターン
	EXTRN	EDGES: WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

; void far _pascal grcg_boxfill( int x1, int y1, int x2, int y2 ) ;
retfunc RETURN
	MRETURN		; ひー、こんなところに！！
endfunc

func GRCG_BOXFILL
	push	BP
	push	SI
	push	DI

	CLI
	add	SP,(RETSIZE+3)*2
	pop	DI	; y2
	pop	SI	; x2
	pop	AX	; y1
	pop	BX	; x1
	sub	SP,(RETSIZE+3+4)*2
	STI

	; まず、クリッピングだよん ==========

	cmp	BX,SI		; BX <= SI にする。
	jle	short L1	;
	xchg	BX,SI		;
L1:				;
	mov	BP,ClipXL
	mov	DX,ClipXW
	sub	SI,BP		; SI < ClipXL なら、範囲外
	jl	short RETURN	;
	sub	BX,BP
	cmp	BX,8000h	; BX < 0 なら、 BX = 0
	sbb	CX,CX		;
	and	BX,CX		;
	sub	SI,DX		; SI >= ClipXW なら、 SI = ClipXW
	sbb	CX,CX		;
	and	SI,CX		;
	add	SI,DX		;
	sub	SI,BX		; BX > SI なら、範囲外
	jl	short RETURN	;
	add	BX,BP		;

	cmp	AX,DI		; y1 > y2 ならば、
	jle short L2		;   y1 <-> y2
	xchg	AX,DI		;
L2:				;
	mov	DX,ClipYT
	mov	BP,ClipYH
	sub	DI,DX		; y2 < ClipYTならば範囲外
	js	short RETURN	;
	sub	AX,DX		; (y1-=ClipYT) < 0 ならば、
	cmp	AX,8000h	;
	sbb	CX,CX		;   y1 = 0
	and	AX,CX		;
	sub	DI,BP		; y2 >= YMAX ならば、
	sbb	CX,CX		;   y2 = YMAX
	and	DI,CX
	add	DI,BP
	sub	DI,AX		; y1 > y2ならば、
	jl	short RETURN	;   範囲外

	; BX = x1(left)
	; SI = abs(x2-x1)
	; AX = y1(upper) - ClipYT
	; DI = abs(y2-y1)

	; 次に、データ作成って感じ。 =========

	mov	DX,AX		; ES = GramSeg + y1 * 5
	shl	AX,2		;
	add	AX,DX		;
	add	AX,ClipYT_seg	;
	mov	ES,AX		;

	mov	DX,DI		; DI = (y2-y1) * 80
	shl	DI,2		;
	add	DI,DX		;
	shl	DI,4		;
	mov	DX,BX		; DI += xl / 16 * 2
	shr	DX,4		;
	shl	DX,1		;
	add	DI,DX		;

	and	BX,0Fh
	add	SI,BX
	sub	SI,10h
	shl	BX,1
	mov	DX,[EDGES+BX]
	not	DX		; DX = first word
	mov	BX,SI
	and	BX,0Fh
	shl	BX,1
	mov	BX,[EDGES+2+BX]	; BX = last word

	sar	SI,4		; SI = num words
	js	short SHORTBOX

	; 現在のデータ (AX,CXはフリー)
	; DX	first word
	; BX	last word
	; SI	num words
	; DI	start address
	; BP	sub offset
	; ES	gseg...

	; 最後、箱塗りだあああああ ===========

; 横幅が２ワード以上あるとき ==============
	lea	BP,[2+40+SI]	; BP = (num words + 2 + 40) * 2
	shl	BP,1		;
	EVEN
YLOOP:
	mov	AX,DX
	stosw
	mov	AX,0FFFFh
	mov	CX,SI
	rep	stosw
	mov	AX,BX
	stosw
	sub	DI,BP
	jnb short YLOOP
	MRETURN
; 横幅が１ワードしかないとき ==============
SHORTBOX:
	mov	BP,82		; BP = 横幅+1ワード
	mov	AX,DX
	and	AX,BX
	EVEN
YLOOP2:
	stosw
	sub	DI,BP
	jnb short YLOOP2
	MRETURN
endfunc

END
