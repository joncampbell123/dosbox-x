; superimpose & master library module
;
; Description:
;	パターンの表示(16x16限定, 4色以内, 画面上下連続)
;
; Functions/Procedures:
;	void super_roll_put_tiny( int x, int y, int num ) ;
;
; Parameters:
;	x,y	描画する座標
;	num	パターン番号
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
;	CPU: V30
;	GRCG
;
; Notes:
;	あらかじめ、super_convert_tiny でデータを変換しておく必要アリ
;	データ形式: (super_patdata[])
;		--------------------------
;		1ワード = (ひとつめの色コード << 8) + 80h
;		32バイト = データ
;		--------------------------
;		1ワード = (ふたつめの色コード << 8) + 80h
;		32バイト = データ
;		…
;		--------------------------
;		1ワード = 0 (終わり)
;
;	色が 4色より多いと、データが元より大きくなるので変換するとき注意
;
;	シューティングゲームなんかで、大量に表示される「弾」は、
;	16x16dot 以内でできるよね?それならこれで高速にできるね
;
;	yの指定可能範囲は、0 ～ 399
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 9/19 Initial: superrpt.asm / master.lib 0.21
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patdata:WORD

WIDE_RANGE	equ 0		; 1 なら yは -400～784対応

PATTERN_HEIGHT	equ 16
SCREEN_HEIGHT	equ 400
SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; 書き込みﾋﾞｯﾄが立っているﾄﾞｯﾄにﾀｲﾙﾚｼﾞｽﾀから書く

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	ret	6
	EVEN
	endm

func SUPER_ROLL_PUT_TINY	; super_roll_put_tiny() {
	mov	BX,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+2)*2
	y	= (RETSIZE+1)*2
	num	= (RETSIZE+0)*2

	mov	CX,SS:[BX+x]
	mov	DI,SS:[BX+y]
	mov	BX,SS:[BX+num]
	shl	BX,1
	mov	DS,super_patdata[BX]

IF WIDE_RANGE ne 0
	rol	DI,1
	sbb	DX,DX
	and	DX,SCREEN_HEIGHT	; 負なら画面縦ぶん下にずらす
	ror	DI,1
	add	DI,DX
ENDIF

	; SCREEN_XBYTES = 80 を想定
	mov	BX,DI
	shl	BX,2
	add	BX,DI
	shl	BX,4
	mov	AX,CX
	and	CX,7		; CL = x & 7
	shr	AX,3
	add	BX,AX		; BX = draw start offset

	xor	SI,SI
	mov	AX,0a800h
	mov	ES,AX

	lodsw
	cmp	AL,80h
	jnz	short RETURN	; １色もないとき(おいおい)

	mov	AL,GC_RMW
	pushf
	CLI
	out	7ch,AL
	popf

	mov	DL,0ffh		; DL = ワード境界マスク
	shr	DL,CL

	test	BX,1
	jnz	short ODD_COLOR_LOOP
	EVEN
	; 偶数アドレス
EVEN_COLOR_LOOP:
	; 色の指定あるね
	REPT 4
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,PATTERN_HEIGHT
	mov	DI,BX
	cmp	DI,(SCREEN_HEIGHT-PATTERN_HEIGHT+1)*SCREEN_XBYTES
	jb	short EVEN_YLOOP2
IF WIDE_RANGE ne 0
	jmp	short EVEN_YLOOP1_ENTRY
ENDIF

	EVEN
EVEN_YLOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES
	dec	CH
IF WIDE_RANGE ne 0
	jz	short EVEN_YLOOP_END
EVEN_YLOOP1_ENTRY:
ENDIF
	cmp	DI,SCREEN_HEIGHT*SCREEN_XBYTES
	jb	short EVEN_YLOOP1

	sub	DI,SCREEN_HEIGHT*SCREEN_XBYTES

	EVEN
EVEN_YLOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short EVEN_YLOOP2

EVEN_YLOOP_END:
	lodsw
	cmp	AL,80h
	je	short EVEN_COLOR_LOOP

	; 正常なデータならば AL=0だから…(ひい)
	out	GC_MODEREG,AL		; grcg off

RETURN:
	MRETURN


	EVEN
	; 奇数アドレス
ODD_COLOR_LOOP:
	; 色の指定あるね
	REPT 4
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,PATTERN_HEIGHT
	mov	DI,BX
	cmp	DI,(SCREEN_HEIGHT-PATTERN_HEIGHT+1)*SCREEN_XBYTES
	jb	short ODD_YLOOP2
IF WIDE_RANGE ne 0
	jmp	short ODD_YLOOP1_ENTRY
ENDIF

	EVEN
ODD_YLOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AL
	xor	AL,DH
	xchg	AH,AL
	mov	ES:[DI+1],AX
	add	DI,SCREEN_XBYTES

	dec	CH
IF WIDE_RANGE ne 0
	jz	short ODD_YLOOP_END
ODD_YLOOP1_ENTRY:
ENDIF
	cmp	DI,SCREEN_HEIGHT*SCREEN_XBYTES
	jb	short ODD_YLOOP1

	sub	DI,SCREEN_HEIGHT*SCREEN_XBYTES

	EVEN
ODD_YLOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AL
	xor	AL,DH
	xchg	AH,AL
	mov	ES:[DI+1],AX
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short ODD_YLOOP2

ODD_YLOOP_END:
	lodsw
	cmp	AL,80h
	je	short ODD_COLOR_LOOP

	; 正常なデータならば AL=0だから…(ひい)
	out	GC_MODEREG,AL		; grcg off
	MRETURN
endfunc			; }

END
