; master library
;
; Description:
;	テキストセミグラフィックの水平16ドットイメージ描画(属性つき)
;
; Function/Procedures:
;	void text_worddota( int x, int y, unsigned image, unsigned dotlen, unsigned atr ) ;
;
; Parameters:
;	x,y	開始座標( x= -15～159, y=0～99 (25行の時) )
;	image	描画する16bitイメージ(最上位ビットが左端)
;		立っているビットの所はセットし、
;		落ちているビットの所はリセットします。
;	dotlen	描画するドット長 (16を超えると、imageを繰り返します)
;	atr	セットする文字属性
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	クリッピングは横方向(-15～159)と、上(0～)だけ行っています。
;	この関数では、全てのビットが落ちたワードの、セミグラフ属性を
;	落としていません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/25 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	10
	EVEN
	endm

retfunc OUTRANGE
	MRETURN
endfunc

func TEXT_WORDDOTA	; text_worddota() {
	push	BP
	mov	BP,SP
	; 引数
	x     = (RETSIZE+5)*2
	y     = (RETSIZE+4)*2
	image = (RETSIZE+3)*2
	dotlen= (RETSIZE+2)*2
	atr   = (RETSIZE+1)*2

	push	SI
	push	DI

	mov	AX,[BP+y]
	mov	BX,AX
	and	BX,3
	xor	AX,BX
	js	short OUTRANGE	; yがマイナスなら範囲外
	mov	CX,AX
	shr	AX,1
	shr	AX,1
	add	AX,CX
	shl	AX,1
	add	AX,TextVramSeg
	mov	ES,AX

	mov	AX,[BP+x]
	cwd
	and	DX,AX
	sub	AX,DX
	mov	DI,AX		; DI = x, xがマイナスなら DI = 0, DX = x
	mov	CX,DX
	neg	CX
	mov	SI,[BP+image]	; SI = image
	and	CL,15
	rol	SI,CL		; 左にはみ出ている場合、そのぶん進める
	not	SI
	mov	CX,BX		; CX = (y % 4)

	mov	BX,[BP+atr]	; BX = 属性
	or	BL,10h		; cemi
	mov	BP,[BP+dotlen]	; BP = dotlen
	mov	AX,160		; 右にはみでる?
	sub	AX,BP
	sub	AX,DI
	add	BP,DX
	cwd
	and	AX,DX
	add	BP,AX		; BP = dot length
	jle	short	OUTRANGE	; 1dotも打てないなら範囲外


	shr	DI,1		; 最初の点のアドレス計算 -> DI
	sbb	CH,CH		; ドット位置 -> CL
	and	CH,4
	or	CL,CH
	shl	DI,1

	mov	AX,0101h
	shl	AX,CL
	mov	CL,4
	rol	AH,CL
	mov	CX,AX		; CL = bit mask, CH = next bit mask

	; ここからが描画だ
XLOOP:
	mov	AX,BX
	xchg	ES:[DI+2000h],AX
	test	AL,10h
	mov	AX,0
	jz	short XCONT
	mov	AX,ES:[DI]
XCONT:
	or	AL,CL		; 足しておいて
	rol	SI,1		; 点を抜くかどうか
	sbb	DL,DL
	and	DL,CL
	xor	AL,DL		; 抜く
	dec	BP
	jle	short LASTWORD

	xchg	CL,CH			; 次のドットだ
	cmp	CL,CH
	ja	short XCONT
	stosw
	jmp	short XLOOP
	EVEN
LASTWORD:
	stosw
;OUTRANGE:
	MRETURN
endfunc

END
