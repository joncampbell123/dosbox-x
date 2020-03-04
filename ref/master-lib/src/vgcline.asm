; master library - VGA - 16color
;
; Description:
;	直線の描画
;
; Functions/Procedures:
;	void vgc_line( int x1, int y1, int y1, int y2 ) ;

; Parameters:
;	int x1,y1	始点の座標
;	int x1,y2	終点の座標
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
;	・grc_setclip()によるクリッピングに対応しています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;	vgc_setcolor()
;	cutline
;
; Revision History:
;	92/6/7	Initial: gc_line.asm
;	92/6/8	長さが1dotのときに/0を起こしていたのを訂正
;		変数を全てレジスタに割り当て
;		水平線を組み込み
;	92/6/9	クリッピングに不備。両方の点が外にあり、片方を
;		切断処理すると、長さが 1dotになってしまう場合、
;		残りの点を切断しようとするときに /0が発生!→訂正
;	92/6/9	ななめ45度ががあん～～～～＠＠＠→訂正
;	92/6/10	コードサイズの最適化
;	92/6/13 少々加速
;	92/6/14
;	92/6/16 TASM対応
;	92/6/22 45度を加速…
;	92/7/27 cutlineをcutline.asmに切り出し
;	94/3/9	Arranged by Ara [svgcline.asm]
;	94/4/8 Initial: vgcline.asm/master.lib 0.23 by 恋塚
;		横640dot以外に対応

	.186
	.MODEL SMALL
	include func.inc
	include clip.inc
	include vgc.inc

	.DATA

	; クリッピング関係
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD

	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	; 水平線用のエッジデータ
	EXTRN	EDGES:WORD

	.CODE

; カットカット～～～(x1,y1)を切断により変更する
; in: BH:o2, BL:o1
; out: BH:o2, BL:o1, o1がzeroならば zflag=1

;
MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

func VGC_LINE	; vgc_line() {
	push	BP
	push	SI
	push	DI

	CLI
	; parameters
	add	SP,(RETSIZE+3)*2
	pop	BP	; y2
	pop	SI	; x2
	pop	DI	; y1
	pop	CX	; x1
	sub	SP,(RETSIZE+3+4)*2
	STI

	; クリッピング開始 =================================

	; クリップ枠に合わせてy座標を変換
	mov	AX,ClipYT
	sub	DI,AX
	sub	BP,AX

	mov	AX,ClipXL
	mov	DX,ClipYH
	mov	BX,0505h	; outcode を全部左下に!!
	GETOUTCODE BL,CX,DI,AX,ClipXR,DX
	GETOUTCODE BH,SI,BP,AX,ClipXR,DX
	test	BH,BL
	jnz	short RETURN1	; まず初期検査。クリップアウト～
	or	BX,BX
	jz	short DRAW_START

	call	cutline		; 第１点のクリップ
	jz	short DRAW_START
	xchg	BH,BL
	xchg	CX,SI
	xchg	DI,BP
	call	cutline		; 第２点のクリップ
	jnz	short RETURN1

	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+

	; 描画開始 =================================
DRAW_START:
	mov	ES,graph_VramSeg	; セグメント設定
	mov	BX,graph_VramWidth

	sub	SI,CX			; SI = abs(x2-x1) : deltax
	jnb	short S400
		add	CX,SI		; CX = leftx
		neg	SI
		xchg	DI,BP
S400:
	mov	AX,DI			; AX = DI * 80
	add	AX,ClipYT
	imul	BX

	sub	BP,DI			;
	sbb	DX,DX			;
	add	BX,DX			; BX(downf) = XBYTES ? -XBYTES
	xor	BX,DX			; DX = abs(y2-y1) : deltay
	add	BP,DX			;
	xor	DX,BP			;

	mov	DI,CX
	shr	DI,3
	add	DI,AX			; DI = start adr!
	xor	AX,AX
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+

	; 縦長か横長かの判定
	cmp	SI,DX
	jg	short YOKO_DRAW1
	je	short NANAME_DRAW
	jmp	short TATE_DRAW

YOKO_DRAW1:
	jmp	YOKO_DRAW

; 縦長の描画 =================================
TATE_DRAW:
	xchg	DX,SI		; DX = deltax, SI = deltay
	div	SI
;	mov	DX,8000h
	mov	BP,AX

	and	CL,07h
	mov	AL,80h		; 80h
	shr	AL,CL

	lea	CX,[SI+1]	; CX = deltay + 1

	; 縦長のループ ~~~~~~~~~~~~~~~~
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !  bit!8000h!downf! len !deltx!gadr !dx/dy!
	; +-----+-----+-----+-----+-----+-----+-----+
	EVEN
	mov	SI,8000h
TATELOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	SI,BP
	jc	short TMIGI
	add	DI,BX
	loop	short TATELOOP
RETURN1:
	MRETURN
TMIGI:	ror	AL,1
	adc	DI,BX
	loop	short TATELOOP
	jmp	short RETURN1

; ナナメ45度の描画 =================================
NANAME_DRAW:
	and	CL,07h
	mov	AL,80h
	shr	AL,CL
	lea	CX,[SI+1]	; CX = deltay + 1

	; 4個ずつループ展開… gr.lib 0.9のline.incでなされていた改変を
	; こちらも追っ掛け(^^; 全く同じじゃ気に入らないから変えたけど…
	shr	CX,1
	jnb	short NANAME1
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
NANAME1:
	jcxz	short NANAMERET
	shr	CX,1
	jnb	short NANAME2
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
NANAME2:
	jcxz	short NANAMERET

	; ななめ45度のループ ~~~~~~~~~~~~~~~~
	EVEN
NANAMELOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX
	loop	short NANAMELOOP
NANAMERET:
	MRETURN

; 横長の描画 =================================
YOKO_DRAW:
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+
	or	DX,DX
	jz	short HLINE_DRAW

	div	SI
	mov	DX,8000h
	mov	BP,DX		; BP = 8000h(dd)
	and	CL,7
	shr	DX,CL		; DX = bit
	mov	CX,SI		;
	inc	CX		; CX = deltax + 1
	mov	SI,AX		; SI = deltay / deltax
	mov	AL,0 		; wdata = 0

	; 横長のループ ~~~~~~~~~~~~~~~~
	EVEN
YOKOLOOP:
	or	AL,DH
	ror	DH,1
	jc	short S200
	add	BP,SI
	jc	short S300
	loop	short YOKOLOOP
	test	ES:[DI],AL
	mov	ES:[DI],AL
YOKORET:
	MRETURN
S200:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	inc	DI
	mov	AL,0
	add	BP,SI
	jc	short S350
	loop	YOKOLOOP
	jmp	short YOKORET
S300:
	test	ES:[DI],AL
	mov	ES:[DI],AL

	mov	AL,0
S350:	add	DI,BX
	loop	short YOKOLOOP
	jmp	short YOKORET
	EVEN

; 水平線の描画 ==============================
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!gadr !     !
	; +-----+-----+-----+-----+-----+-----+-----+
HLINE_DRAW:
	mov	BX,CX		; BX = leftx & 7
	and	BX,7		;
	lea	CX,[BX+SI-8]	; CX = deltax + BX - 8
	shl	BX,1
	mov	AL,byte ptr [EDGES+BX]	; 左エッジ
	not	AL

	mov	BX,CX		;
	and	BX,7
	shl	BX,1

	sar	CX,3
	js	short HL_LAST
	test	ES:[DI],AL
	mov	ES:[DI],AL
	inc	DI

	mov	AL,0FFh
	jcxz	short HL_LAST
HLINE_REP:
	or	ES:[DI],AL
	inc	DI
	loop	short HLINE_REP
HL_LAST:
	and	AL,byte ptr [EDGES+2+BX]	; 右エッジ
	test	ES:[DI],AL
	mov	ES:[DI],AL
	MRETURN
endfunc		; }
END
