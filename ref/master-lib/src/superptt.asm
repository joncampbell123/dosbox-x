; superimpose & master library module
;
; Description:
;	パターンの表示(横16dot限定, 4色以内)
;
; Functions/Procedures:
;	void super_put_tiny( int x, int y, int num ) ;
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
;	★縦ドット数=n
;	あらかじめ、super_convert_tiny でデータを変換しておく必要アリ
;	データ形式: (super_patdata[])
;		--------------------------
;		1ワード = (ひとつめの色コード << 8) + 80h
;		n*2バイト = データ
;		--------------------------
;		1ワード = (ふたつめの色コード << 8) + 80h
;		n*2バイト = データ
;		…
;		--------------------------
;		1ワード = 0 (終わり)
;
;	色が 4色より多いと、データが元より大きくなるので変換するとき注意
;
;	シューティングゲームなんかで、大量に表示される「弾」は、
;	16x n dot 以内でできるよね?それならこれで高速にできるね
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 9/18 Initial: superptt.asm / master.lib 0.21
;	94/ 8/16 [M0.23] 縦ドット数を可変に
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	super_patsize:WORD
	EXTRN	super_patdata:WORD

SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; 書き込みﾋﾞｯﾄが立っているﾄﾞｯﾄにﾀｲﾙﾚｼﾞｽﾀから書く

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
	endm

func SUPER_PUT_TINY	; super_put_tiny() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg
	mov	BX,[BP+num]
	mov	CX,[BP+x]
	shl	BX,1
	mov	DI,[BP+y]
	mov	BP,super_patsize[BX]
	mov	DS,super_patdata[BX]

	mov	DX,DI
	shl	DI,2
	add	DI,DX
	shl	DI,4
	mov	DX,CX
	and	CX,7		; CL = x & 7
	shr	DX,3
	add	DI,DX		; DI = draw start offset

	xor	SI,SI

	lodsw
	cmp	AL,80h
	jnz	short RETURN	; １色もないとき(おいおい)

	mov	AL,GC_RMW
	pushf
	CLI
	out	GC_MODEREG,AL
	popf

	mov	DL,0ffh		; DL = ワード境界マスク
	shr	DL,CL

	mov	BX,BP			; BL=PATTERN_HEIGHT
	mov	BP,AX
	mov	AL,SCREEN_XBYTES
	mul	BL
	xchg	BP,AX			; BP=PATTERN_HEIGHT * SCREEN_XBYTES

	test	DI,1
	jnz	short ODD_COLOR_LOOP
	EVEN
	; 偶数アドレス
EVEN_COLOR_LOOP:
	shr	AH,1	; 色の指定あるね
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL

	mov	CH,BL
	EVEN
EVEN_YLOOP:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short EVEN_YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80h
	je	short EVEN_COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off

RETURN:
	MRETURN

	EVEN
	; 奇数アドレス
ODD_COLOR_LOOP:
	shr	AH,1	; 色の指定あるね
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL

	mov	CH,BL
	EVEN
ODD_YLOOP:
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
	jnz	short ODD_YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80h
	je	short ODD_COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off
	MRETURN
endfunc			; }

END
