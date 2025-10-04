PAGE 98,120
; graphics - grcg - hline - PC98V
;
; DESCRIPTION:
;	直線の描画(青プレーンのみ)
;
; FUNCTION:
;	void far _pascal grcg_line( int x1, int y1, int y1, int y2 ) ;

; PARAMETERS:
;	int x1,y1	始点の座標
;	int x1,y2	終点の座標
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリッピングに対応しています。
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;	clipline
;
; HISTORY:
;	92/6/7	Initial
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

	.186
	.MODEL SMALL

	.DATA

	; クリッピング関係
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	; 水平線用のエッジデータ
	EXTRN	EDGES:WORD

	.CODE
	include func.inc
	include clip.inc

ifndef X1280
XBYTES	= 80
YSHIFT4	= 4
else
XBYTES	= 160
YSHIFT4	= 5
endif


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

;-------------------------------------------------------------
; void far _pascal grcg_line( int x1, int y1, int x2, int y2 ) ;
;
func GRCG_LINE
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
	; セグメント設定
	mov	ES,ClipYT_seg

	sub	SI,CX			; SI = abs(x2-x1) : deltax
	jnb	short S400
		add	CX,SI		; CX = leftx
		neg	SI
		xchg	DI,BP
S400:
	sub	BP,DI			;
	sbb	DX,DX			;
	mov	BX,XBYTES		; BX(downf) = XBYTES ? -XBYTES
	add	BX,DX			; DX = abs(y2-y1) : deltay
	xor	BX,DX			;
	add	BP,DX			;
	xor	DX,BP			;

	mov	AX,DI			; DI *= 80
	shl	AX,2
	add	DI,AX
	shl	DI,YSHIFT4

	mov	AX,CX
	shr	AX,3
	add	DI,AX			; DI = start adr!
	xor	AX,AX
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+

	; 縦長か横長かの判定
	cmp	SI,DX
	jg	short YOKO_DRAW
	je	short NANAME_DRAW

; 縦長の描画 =================================
TATE_DRAW:
	dec	BX		; BX = downf - 1

	xchg	DX,SI		; DX = deltax, SI = deltay
	div	SI
	mov	DX,8000h
	mov	BP,AX

	and	CL,07h
	mov	AL,DH		; 80h
	shr	AL,CL

	lea	CX,[SI+1]	; CX = deltay + 1

	; 縦長のループ ~~~~~~~~~~~~~~~~
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !  bit!8000h!downf! len !deltx!gadr !dx/dy!
	; +-----+-----+-----+-----+-----+-----+-----+
	EVEN
TATELOOP:
	stosb
	add	DX,BP
	jc	short TMIGI
	add	DI,BX
	loop	short TATELOOP
RETURN1:
	MRETURN
TMIGI:	ror	AL,1
	adc	DI,BX
	loop	short TATELOOP
	MRETURN

; ナナメ45度の描画 =================================
NANAME_DRAW:
	dec	BX		; BX = downf - 1
	and	CL,07h
	mov	AL,80h
	shr	AL,CL
	lea	CX,[SI+1]	; CX = deltay + 1

	; 4個ずつループ展開… gr.lib 0.9のline.incでなされていた改変を
	; こちらも追っ掛け(^^; 全く同じじゃ気に入らないから変えたけど…
	shr	CX,1
	jnb	short NANAME1
	stosb
	ror	AL,1
	adc	DI,BX
NANAME1:
	jcxz	short NANAMERET
	shr	CX,1
	jnb	short NANAME2
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
NANAME2:
	jcxz	short NANAMERET

	; ななめ45度のループ ~~~~~~~~~~~~~~~~
	EVEN
NANAMELOOP:
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
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
	and	DI,0FFFEh

	or	DX,DX
	jz	short HLINE_DRAW

	div	SI
	mov	DX,8000h
	mov	BP,DX		; BP = 8000h(dd)
	and	CL,0Fh
	shr	DX,CL		; DX = bit
	mov	CX,SI		;
	inc	CX		; CX = deltax + 1
	mov	SI,AX		; SI = deltay / deltax
	xor	AX,AX		; AX = 0(wdata)

	; 横長のループ ~~~~~~~~~~~~~~~~
	EVEN
YOKOLOOP:
	or	AX,DX
	ror	DX,1
	jc	short S200
	add	BP,SI
	jc	short S300
	loop	YOKOLOOP
	xchg	AH,AL
	stosw
	MRETURN
S200:	xchg	AH,AL
	stosw
	xor	AX,AX
	add	BP,SI
	jc	short S350
	loop	YOKOLOOP
	MRETURN
S300:	xchg	AH,AL
	mov	ES:[DI],AX
	xor	AX,AX
S350:	add	DI,BX
	loop	YOKOLOOP
	MRETURN

; 水平線の描画 ==============================
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!gadr !     !
	; +-----+-----+-----+-----+-----+-----+-----+
HLINE_DRAW:
	mov	BX,CX		; BX = leftx & 15
	and	BX,0Fh		;
	lea	CX,[BX+SI-10h]	; CX = deltax + BX - 16
	shl	BX,1
	mov	AX,[EDGES+BX]	; 左エッジ
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short HL_LAST
	stosw
	mov	AX,0FFFFh
	rep stosw
HL_LAST:
	and	AX,[EDGES+2+BX]	; 右エッジ
	stosw
	MRETURN
endfunc
END
