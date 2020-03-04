; master library module
;
; Description:
;	画面上の四角形領域の移動
;
; Functions/Procedures:
;	void graph_move( int x1, int y1, int x2, int y2, int nx, int ny ) ;
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	入力座標の正当性検査は一切行っていません。
;	クリッピングも行っていません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/15 Initial
;	93/ 6/ 3 [M0.19] ･画面左上端からの移動 ･横幅が8dot未満 各bug fix
;

	.186
	.MODEL SMALL
	include func.inc
	.CODE

; in:
;	stack = dmask
;	SI = sadd
;	DI = tadd
;	CL = shiftdif
;	AX = y len
;	DX = xlen
; break:
;	AX,DX,BX,CX, SI,DI,BP, DS,ES
GREV	PROC NEAR	; {
	mov	BP,SP
	push	AX	; ylen = -2

	lmask	= 2
	rmask	= 3

	ylen	= -2

	inc	DI		; tp

GREV_LOOPY:
	mov	AX,0a800H

GREV_LOOPSEG:
	add	SI,DX
	add	DI,DX

	mov	DS,AX
	mov	BX,[SI]
	xchg	BH,BL		; w

	mov	CH,[BP+rmask]
	test	CH,CH
	je	short GREV_SKIP_R

	mov	AX,BX		; w
	shr	AX,CL
	and	AL,CH
	not	CH
	and	CH,[DI]
	or	CH,AL
	mov	[DI],CH
GREV_SKIP_R:
	test	DX,DX		; xlen
	jle	short GREV_L

	mov	CH,BH
	mov	BX,DX		; xlen
GREV_LOOPX:
	dec	SI
	mov	AL,CH
	mov	AH,[SI]
	mov	CH,AH
	shr	AX,CL
	dec	DI
	mov	[DI],AL
	dec	BX		; xlen
	jne	short GREV_LOOPX
	mov	BH,CH

GREV_L:
	mov	BL,BH
	mov	BH,[SI-1]

	mov	CH,[BP+lmask]
	shr	BX,CL
	mov	AL,CH
	and	AL,BL
	not	CH
	and	CH,[DI-1]
	or	CH,AL
	mov	[DI-1],CH

	mov	AX,DS
	add	AH,8
	or	AH,20h
	cmp	AX,0e800h
	jb	short GREV_LOOPSEG

	add	SI,80
	add	DI,80

	dec	WORD PTR [BP+ylen]
	js	short GREV_EXIT
	jmp	GREV_LOOPY
GREV_EXIT:

	pop	AX
	ret	2
GREV	ENDP		; }


; in: 
;	stack top: dmask
;	AX: y len
;	CL: shiftdif
;	BX: yplus (1line処理毎の y方向変位。下なら+80, 上なら-80)
;	DX: xlen (byte単位の長さ)
;	SI: sadd
;	DI: tadd
; out:
; break:
;	AX,DX,BX,CX, SI,DI,BP, DS,ES
;
GMOVE	PROC NEAR	; {
	mov	BP,SP

	; 引数
	dmask	= 2

	mov	CS:GMOVE_YPLUS,BX
	mov	BX,[BP+dmask]
	mov	CS:GMOVE_LMASK,BL
	mov	CS:GMOVE_RMASK,BH

	mov	BP,AX
	test	BP,BP
	jge	short GMOVE_LOOPLINE	; pre-fetch queue clear
	jmp	short GMOVE_RET
	EVEN
GMOVE_LOOPLINE:
	mov	AX,0a800H

GMOVE_LOOPSEG:
	mov	ES,AX
	mov	DS,AX

	mov	BH,[SI-1]
	mov	BL,[SI]
	inc	SI		; fp

	mov	AX,BX
	shr	AX,CL
	mov	CH,0		; dummy
	org	$-1
GMOVE_LMASK	db	?

	and	AL,CH
	not	CH
	and	CH,[DI]
	or	AL,CH
	stosb

	or	DX,DX		; xlen
	jle	short GMOVE_RIGHT
	; xlenが 1以上

	test	CL,CL
	jnz	short GMOVE_SHIFT

	; shiftdifがゼロのとき

	mov	BH,CL
	mov	CX,DX
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	mov	CL,BH
	jmp	SHORT GMOVE_RIGHT	; CX = 0

	; shiftdifがゼロ以外のとき
GMOVE_SHIFT:
	mov	CH,BL
	mov	BX,DX
GMOVE_LOOPX:
	mov	AH,CH
	lodsb
	mov	CH,AL
	shr	AX,CL
	stosb
	dec	BX
	jne	short GMOVE_LOOPX
	mov	BL,CH

GMOVE_RIGHT:
	mov	AL,0
	org	$-1
GMOVE_RMASK	db	?
	test	AL,AL
	jz	short GMOVE_RSKIP

	mov	BH,BL
	mov	BL,[SI]

	mov	AH,AL
	shr	BX,CL
	and	AL,BL
	not	AH
	and	AH,[DI]
	or	AH,AL
	mov	[DI],AH

GMOVE_RSKIP:
	mov	AX,DX
	inc	AX
	sub	SI,AX
	sub	DI,AX

	mov	AX,DS
	add	AH,8
	or	AH,20h
	cmp	AX,0e800h
	jb	short GMOVE_LOOPSEG

	mov	AX,1234h	; dummy
	org	$-2
GMOVE_YPLUS	dw	?
	add	SI,AX
	add	DI,AX

	dec	BP
	jns	short GMOVE_LOOPLINE

GMOVE_RET:
	ret	2
	EVEN
GMOVE	ENDP		; }

MRETURN macro
	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	12
	EVEN
	endm

func GRAPH_MOVE		; {	(MSCの出力をちょっといぢっただけだし)
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; 引数
	x1 = (RETSIZE+6)*2
	y1 = (RETSIZE+5)*2
	x2 = (RETSIZE+4)*2
	y2 = (RETSIZE+3)*2
	nx = (RETSIZE+2)*2
	ny = (RETSIZE+1)*2

	mov	BX,[BP+y2]	; BX = y length
	sub	BX,[BP+y1]

	mov	DI,[BP+ny]
	cmp	[BP+y1],DI
	jge	short L1
	add	DI,BX
L1:
	mov	AX,[BP+nx]
	sar	AX,3
	imul	DI,DI,80
	add	DI,AX		; DI = tadd

	mov	CH,[BP+nx]
	and	CH,7		; CH = shiftbit
	mov	CL,8		; CL = 8
	mov	AX,[BP+x2]
	sub	AX,[BP+x1]
	inc	AX
	cmp	AX,8
	jge	short THICK

	; xlen < 8 のとき
	sub	CL,AL
	mov	AX,00ffh
	shl	AL,CL
	mov	CL,CH		; CL = shiftbit
	ror	AX,CL		; AL= 0ffh << (8-xlen) >> shiftbit  AH= 0
	mov	DX,0		; DX = xlen = 0
	jmp	short NORMAL
	EVEN

	; xlen >= 8 のとき
THICK:	; AX = xlen
	add	AL,CH		; AX = AX + shiftbit - 8
	adc	AH,0
	sub	AX,8
	mov	DX,AX
	shr	DX,3		; DX = xlen
	div	CL
	mov	CL,AH
	mov	AX,00ffh
	ror	AX,CL
	mov	AL,0ffh
	mov	CL,CH		; CL = shiftbit
	shr	AL,CL		; AL=0ffh>>shiftbit AH=(0ff00h >> dx)

NORMAL:
	push	AX		; push AL(lmask),AH(rmask)

	mov	AX,[BP+x1]
	shr	AX,3
	imul	SI,WORD PTR [BP+y1],80
	add	SI,AX		; SI = sadd

	mov	AL,[BP+x1]
	and	AL,7
	sub	CL,AL		; CL = shiftbit - (x1 & 7)
	jns	short NO_SHIFT_LEFT
	add	CL,8
	inc	SI
NO_SHIFT_LEFT:
	; stack top (L = lmask  H = rmask)
	; DX = xlen
	; CL = shiftdif
	; BX = y length
	; SI = sadd
	; DI = tadd
	mov	AX,BX		; y length

	mov	BX,[BP+ny]
	cmp	[BP+y1],BX
	jl	short LOWER
	jne	short LEFT_UPPER

	; 水平に移動するとき
	mov	BX,[BP+nx]
	cmp	[BP+x1],BX
	jg	short LEFT_UPPER

	; 水平の右に移動するとき
	call	GREV
	MRETURN

	; 下に移動するとき
LOWER:
	imul	BX,AX,80
	add	SI,BX
	mov	BX,-80
	jmp	short UPPERLOWER
	EVEN
	; 水平左または、上に移動するとき
LEFT_UPPER:
	mov	BX,80
UPPERLOWER:
	call	GMOVE
	MRETURN
endfunc			; }

END
