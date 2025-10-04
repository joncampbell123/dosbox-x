; master library - VGA - 16color - boxfill
;
; Description:
;	VGA 16色 箱塗り
;
; Functions/Procedures:
;	void vgc_boxfill( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	int x1,y2	第１点
;	int x2,y2	第２点
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・grc_setclip()によるクリッピングを行っています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; 関連関数:
;	grc_setclip(), vgc_setcolor()
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/ 3 Initial: vgcboxfi.asm/master.lib 0.22
;	94/ 4/ 8 [M0.23] 横640dot以外にも対応

	.186
	.MODEL SMALL

	.DATA

	; grc_setclip()関連
	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	; エッジパターン
	EXTRN	EDGES: WORD

	.CODE
	include func.inc
	include vgc.inc

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

retfunc RETURN
	MRETURN		; ひー、こんなところに！！
endfunc

func VGC_BOXFILL	; vgc_boxfill() {
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
	and	CX,DI		; CX=DI & CX
	add	CX,BP
	sub	CX,AX		; y1 > y2ならば、
	jl	short RETURN	;   範囲外

	inc	CX
	add	AX,DX

	; BX = x1(left)
	; SI = abs(x2-x1)
	; AX = y1(upper)
	; CX = abs(y2-y1)+1

	; 次に、データ作成って感じ。 =========

	mov	ES,graph_VramSeg ; ES = graph_VramSeg

	mov	DI,BX		; DI += xl / 8
	shr	DI,3
	imul	graph_VramWidth
	add	DI,AX

	and	BX,07h
	lea	SI,[SI+BX-8]
	shl	BX,1
	mov	AL,byte ptr EDGES[BX]
	not	AX		; AH = first byte
	mov	BX,SI
	and	BX,07h
	shl	BX,1
	mov	BH,byte ptr EDGES[2+BX]	; BH = last byte

	mov	DX,graph_VramWidth

	sar	SI,3		; SI = num bytes
	js	short RIGHT_EDGE

	; 現在のデータ (AH,CXはフリー)
	; AL	first byte
	; BH	last byte
	; DX	graph_VramWidth
	; SI	num words
	; DI	start address
	; ES	gseg...
	; CX	abs(y2-y1)+1

	; 最後、箱塗りだあああああ ===========

	mov	BP,DI
	push	CX
	EVEN
LEFT_EDGE:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	DI,DX
	loop	short LEFT_EDGE
	pop	CX

	inc	BP
	mov	DI,BP

	test	SI,SI
	jz	short RIGHT_EDGE1
	EVEN
	mov	AX,BX
	push	CX
BODY:
	mov	BX,SI
REPSTOSB:
	or	byte ptr ES:[DI+BX-1],0ffh
	dec	BX
	jnz	short REPSTOSB
	add	DI,DX
	loop	short BODY
	pop	CX
	mov	BX,AX

	lea	DI,[BP+SI]
RIGHT_EDGE1:
	mov	AL,0ffh

RIGHT_EDGE:
	and	AL,BH
	EVEN
RIGHTLOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	DI,DX
	loop	short RIGHTLOOP
	MRETURN
endfunc			; }

END
