; master library - 
;
; Description:
;	GDCによる円描画
;
; Function/Procedures:
;	void gdc_circle( int x, int y, unsigned r ) ;
;
; Parameters:
;	x,y	中心座標
;	r	半径
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GDC
;
; Notes:
;	
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
;	93/ 5/13 Initial:gdccircl.asm/master.lib 0.16

	.186
	.MODEL SMALL
	include func.inc

	.CODE

SQRT2_16 equ 46341	; 65536/sqrt(2)

	.DATA
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD

	EXTRN	GDCUsed:WORD
	EXTRN	GDC_LineStyle:WORD
	EXTRN	GDC_AccessMask:WORD
	EXTRN	GDC_Color:WORD

	.DATA?

DGD	DW ?
DotAdr	DB ?
Dir	DB ?

	.CODE

	EXTRN	ISQRT:CALLMODEL
	EXTRN	GDC_WAITEMPTY:CALLMODEL
	EXTRN	gdc_outpw:NEAR


GDC_PSET equ 0
GDC_XOR	equ 1
GDC_AND	equ 2
GDC_OR	equ 3

GDC_CMD   equ 0a2h
GDC_PRM   equ 0a0h
GDC_STAT  equ 0a0h

; GDC modify mode
REPLACE	   equ 020h
COMPLEMENT equ 021h
CLEAR	   equ 022h
SET	   equ 023h

VECTW	equ 4ch
VECTE	equ 6ch
TEXTW	equ 78h
CSRW 	equ 49h

; IN:
;	SI : N(描画総ドット数)
;	BP : r-1
;	DI : M(マスキングドット数)
arcdelta proc near
	call	GDC_WAITEMPTY		; FIFOが空になるのを待つ
	mov	AL,VECTW
	out	GDC_CMD,AL		; [C 1]

	mov	AL,Dir
	out	GDC_PRM,AL		; [P 1]

	mov	AX,SI
	or	AX,DGD
	call	gdc_outpw		; [P 2]

	mov	AX,BP
	call	gdc_outpw		; [P 2]

	mov	AX,BP
	shl	AX,1
	call	gdc_outpw		; [P 2]

	mov	AX,-1
	call	gdc_outpw		; [P 2]

	mov	AX,DI
	call	gdc_outpw		; [P 2]
	ret
	EVEN
arcdelta endp


; IN:
;	AX: top address
; BREAK:
;	AX
drawarc	proc near
	push	AX
	call	GDC_WAITEMPTY		; FIFOが空になるのを待つ
	mov	AL,CSRW
	out	GDC_CMD,AL		; [C 1]

	pop	AX
	call	gdc_outpw		; [P 2]
	mov	AL,DotAdr
	out	GDC_PRM,AL		; [P 1]

	mov	AL,VECTE
	out	GDC_CMD,AL		; [C 1]

	ret
	EVEN
drawarc	endp

; 8分の1円弧描画
; in:
;	AL = dir
;	BX = r
;	SI = x
;	DI = y
; break:
;	AX,BX,CX,DX,SI,DI
ARC8	proc near
	push	BP
	mov	BP,SP
	e	= (RETSIZE+2)*2
	s	= (RETSIZE+1)*2

	or	AL,20h	; arc
	mov	Dir,AL
	and	AL,7

	;      61
	;     3  4
	;     0  7
	;      52

	mov	AH,AL
	add	AH,11111100b
	sbb	CX,CX
	test	AH,3	; 00,11=even,01,10=odd
	jpo	short A8_1
	mov	AX,BX		; dir = 0,3: SI = x - r
	xor	AX,CX		; dir = 4,7: SI = x + r
	sub	AX,CX
	sub	SI,AX		; SI = x - r
	jmp	short A8_9
	EVEN
A8_1:
	cmp	AL,1
	je	short A8_2
	cmp	AL,6
	jne	short A8_3
A8_2:
	sub	DI,BX		; dir = 1,6: DI = y - r
	jmp	short A8_9
	EVEN
A8_3:
	add	DI,BX		; dir = 2,5: DI = y + r
A8_9:
	; in: SI = x1
	;     DI = y1
	mov	DX,DI		; DX = (y1*40)+(x1/16)
	mov	AX,SI		;      (GDC ADDRESS)
	shl	DX,2		;
	add	DX,DI		;
	shl	DX,3		;
	shr	SI,4		;
	add	DX,SI		;

	shl	AL,4
	mov	DotAdr,AL

	mov	SI,[BP+e]
	mov	DI,[BP+s]

	lea	BP,[BX-1]
	; SI : N(描画総ドット数)
	; BP : r-1
	; DI : M(マスキングドット数)
	call	arcdelta

	mov	BX,GDC_AccessMask
	not	BL
	and	BL,0fh
	mov	CX,GDC_Color
	dec	CH
	jns	short XORDRAW

	; PSET描画 -----------------------
NORMALDRAW:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short RED

	out	GDC_CMD,AL
	mov	AX,DX
	or	AH,40h	; GDC VRAM ADDRESS: Blue
	call	drawarc
	call	arcdelta
RED:

	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short GREEN

	out	GDC_CMD,AL
	mov	AX,DX
	or	AH,80h	; GDC VRAM ADDRESS: Red
	call	drawarc
	call	arcdelta
GREEN:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short INTENSITY

	out	GDC_CMD,AL
	mov	AX,DX
	or	AH,0C0h	; GDC VRAM ADDRESS: Green
	call	drawarc
	call	arcdelta

INTENSITY:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short A8_OWARI

	out	GDC_CMD,AL
	jmp	short LLAST
	EVEN

	; XOR,AND,OR描画 -------------------
XORDRAW:
	mov	AL,CH
	add	AL,COMPLEMENT
	out	GDC_CMD,AL

	cmp	CH,GDC_AND-1
	jne	short NOTAND
	not	CL
NOTAND:
	and	CL,BL
	jz	short A8_OWARI

	shr	CL,1
	jnb	short XRED
	mov	AX,DX
	or	AH,40h	; GDC VRAM ADDRESS: Blue
	call	drawarc
	call	arcdelta
XRED:
	shr	CL,1
	jnb	short XGREEN
	mov	AX,DX
	or	AH,80h	; GDC VRAM ADDRESS: Red
	call	drawarc
	call	arcdelta
XGREEN:
	shr	CL,1
	jnb	short XINTENSITY
	mov	AX,DX
	or	AH,0C0h	; GDC VRAM ADDRESS: Green
	call	drawarc
	call	arcdelta
XINTENSITY:
	shr	CL,1
	jnb	short A8_OWARI
LLAST:
	mov	AX,DX	; GDC VRAM ADDRESS: Intensity
	call	drawarc
A8_OWARI:
	pop	BP
	ret	4
	EVEN
ARC8	ENDP


; AX=r,BX=xのときのyをAXにいれる
KOUTEN	proc near
	imul	AX
	xchg	AX,BX
	mov	CX,DX
	imul	AX
	sub	BX,AX
	sbb	CX,DX
	push	CX
	push	BX
	call	ISQRT
	ret
KOUTEN	endp



MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	6
	EVEN
	endm

retfunc OWARI
	MRETURN
endfunc

func GDC_CIRCLE		; {
	enter	32,0
	push	SI
	push	DI

	; 引数
	x = (RETSIZE+3)*2
	y = (RETSIZE+2)*2
	r = (RETSIZE+1)*2

	ko	= -32	; 4*8ｺ

	mov	AX,[BP+y]
	mov	BX,AX
	mov	CX,[BP+r]
	jcxz	short OWARI	; 半径 0 はなにもしない
	sub	AX,CX
	cmp	AX,ClipYB
	jg	short OWARI
	add	BX,CX
	cmp	BX,ClipYT
	jl	short OWARI
	mov	AX,[BP+x]
	mov	BX,AX
	sub	AX,CX
	cmp	AX,ClipXR
	jg	short OWARI
	add	BX,CX
	cmp	BX,ClipXL
	jl	short OWARI

	; 初期設定
	mov	AX,SQRT2_16
	mul	CX
	mov	DI,DX			; DI = r*sin(45ﾟ)

	mov	CX,8
	mov	SI,0
	mov	AX,DI
	inc	AX
INITLOOP:
	mov	WORD PTR [BP+ko+SI],0	; mask
	mov	[BP+ko+SI+2],AX		; total
	add	SI,4
	loop	short INITLOOP


	mov	SI,11111111b		; SI = drawmap

	; クリップ枠上辺との交差判定
	mov	AX,[BP+y]
	mov	BX,AX
	mov	CX,[BP+r]
	sub	AX,CX
	cmp	AX,ClipYT
	jge	short CLIPBOTTOM

	mov	AX,BX
	sub	AX,DI
	cmp	AX,ClipYT
	jle	short CU_1

	mov	AX,CX
	sub	BX,ClipYT
	call	KOUTEN
	mov	[BP+ko+24],AX	; [6].mask
	mov	[BP+ko+4],AX	; [1].mask

	jmp	short CLIPBOTTOM
	EVEN

CU_1:
	and	SI,10111101b	; 1,6 off

	mov	AX,ClipYT
	mov	BX,[BP+y]
	cmp	BX,AX
	jle	short CU_2

	sub	AX,[BP+y]
	neg	AX
	mov	[BP+ko+18],AX	; [4].total
	mov	[BP+ko+14],AX	; [3].total

	jmp	short CLIPBOTTOM
	EVEN
CU_2:
	and	SI,11100111b	; 3,4 off

	mov	AX,DI
	add	AX,BX
	cmp	AX,ClipYT
	jle	short CU_3

	mov	AX,ClipYT
	sub	AX,[BP+y]
	mov	[BP+ko+28],AX	; [7].mask
	mov	[BP+ko],AX	; [0].mask

	jmp	short CLIPBOTTOM
	EVEN
CU_3:

	and	SI,01111110b	; 0,7 off

	mov	BX,ClipYT
	sub	BX,[BP+y]
	mov	AX,CX
	call	KOUTEN
	mov	[BP+ko+22],AX	; [5].total
	mov	[BP+ko+10],AX	; [2].total

CLIPBOTTOM:
	; クリップ枠下辺との交差判定
	mov	AX,[BP+r]
	add	AX,[BP+y]
	cmp	AX,ClipYB
	jle	short CLIPLEFT

	mov	AX,DI
	add	AX,[BP+y]
	cmp	AX,ClipYB
	jge	short CB_1

	mov	BX,ClipYB
	sub	BX,[BP+y]
	mov	AX,[BP+r]
	call	KOUTEN
	mov	[BP+ko+20],AX
	mov	[BP+ko+8],AX

	jmp	short CLIPLEFT
	EVEN
CB_1:

	and	SI,11011011b

	mov	AX,ClipYB
	cmp	[BP+y],AX
	jge	short CB_2

	sub	AX,[BP+y]
	mov	[BP+ko+30],AX
	mov	[BP+ko+2],AX

	jmp	short CLIPLEFT
	EVEN
CB_2:

	and	SI,01111110b

	mov	AX,[BP+y]
	sub	AX,DI
	cmp	AX,ClipYB
	jge	short CB_3

	mov	AX,[BP+y]
	sub	AX,ClipYB
	mov	[BP+ko+16],AX
	mov	[BP+ko+12],AX

	jmp	short CLIPLEFT
	EVEN
CB_3:

	and	SI,11100111b

	mov	BX,[BP+y]
	sub	BX,ClipYB
	mov	AX,[BP+r]
	call	KOUTEN
	mov	[BP+ko+26],AX
	mov	[BP+ko+6],AX

CLIPLEFT:
	; クリップ枠左辺との交差判定
	mov	AX,[BP+x]
	mov	CX,[BP+r]
	sub	AX,CX
	cmp	AX,ClipXL
	jge	short CLIPRIGHT

	mov	AX,[BP+x]
	sub	AX,DI
	cmp	AX,ClipXL
	jle	short CL_1

	mov	AX,CX
	mov	BX,[BP+x]
	sub	BX,ClipXL
	call	KOUTEN
	mov	[BP+ko+12],AX
	mov	[BP+ko],AX

	jmp	short CLIPRIGHT
	EVEN
CL_1:
	and	SI,11110110b

	mov	AX,ClipXL
	cmp	[BP+x],AX
	jle	short CL_2

	sub	AX,[BP+x]
	neg	AX
	mov	[BP+ko+26],AX
	mov	[BP+ko+22],AX

	jmp	short CLIPRIGHT
	EVEN
CL_2:
	and	SI,10011111b

	mov	AX,DI
	add	AX,[BP+x]
	cmp	AX,ClipXL
	jle	short CL_3

	mov	AX,ClipXL
	sub	AX,[BP+x]
	mov	[BP+ko+8],AX
	mov	[BP+ko+4],AX

	jmp	short CLIPRIGHT
	EVEN
CL_3:
	and	SI,11111001b

	mov	BX,ClipXL
	sub	BX,[BP+x]
	mov	AX,CX
	call	KOUTEN
	mov	[BP+ko+30],AX
	mov	[BP+ko+18],AX

CLIPRIGHT:
	; クリップ枠右辺との交差判定
	mov	AX,[BP+r]
	add	AX,[BP+x]
	cmp	AX,ClipXR
	jle	short DRAW_START

	mov	AX,DI
	add	AX,[BP+x]
	cmp	AX,ClipXR
	jge	short CR_1

	mov	BX,ClipXR
	sub	BX,[BP+x]
	mov	AX,[BP+r]
	call	KOUTEN
	mov	[BP+ko+28],AX
	mov	[BP+ko+16],AX

	jmp	short DRAW_START
	EVEN
CR_1:

	and	SI,01101111b

	mov	AX,ClipXR
	cmp	[BP+x],AX
	jge	short CR_2

	sub	AX,[BP+x]
	mov	[BP+ko+10],AX
	mov	[BP+ko+6],AX

	jmp	short DRAW_START
	EVEN
CR_2:
	and	SI,11111001b

	mov	AX,[BP+x]
	sub	AX,DI
	cmp	AX,ClipXR
	jge	short CR_3

	mov	AX,[BP+x]
	sub	AX,ClipXR
	mov	[BP+ko+24],AX
	mov	[BP+ko+20],AX

	jmp	short DRAW_START
	EVEN
CR_3:
	and	SI,10011111b

	mov	BX,[BP+x]
	sub	BX,ClipXR
	mov	AX,[BP+r]
	call	KOUTEN
	mov	[BP+ko+12],AX
	mov	[BP+ko],AX

DRAW_START:

	; ラインスタイルとDGDの設定
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[054dh],4	; NZ = GDC 5MHz
	jz	short L100		; Z = 2.5MHz
	mov	DGD,4000h
	jmp	short L200
	EVEN
L100:
	mov	DGD,0
	cmp	GDCUsed,0
	je	short L200
	call	GDC_WAITEMPTY
L200:
	mov	AL,TEXTW
	out	GDC_CMD,AL		; ラインスタイルの設定 [ C 1 ]

	mov	AX,GDC_LineStyle
	call	gdc_outpw		; ラインスタイルの設定 [ P 2 ]


	; 8分の1円弧単位で一周するループ

	xor	DI,DI		; DI breaks
	shr	SI,1
	jnc	short SKIP_ARC

ROUNDLOOP:
	push	SI
	push	DI
	mov	AX,DI
	shr	AX,2
	push	[BP+ko+DI+2]	; total draw len
	push	[BP+ko+DI]	; mask len
	mov	BX,[BP+r]
	mov	SI,[BP+x]
	mov	DI,[BP+y]
	call	ARC8
	pop	DI
	pop	SI

SKIP_ARC:
	add	DI,4
	shr	SI,1
	jb	short ROUNDLOOP
	jnz	short SKIP_ARC

	mov	GDCUsed,1

	MRETURN
endfunc		; }

END
