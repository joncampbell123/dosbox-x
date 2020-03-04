; superimpose & master library module
;
; Description:
;	パターンの表示(8xn限定, 4色以内)
;
; Functions/Procedures:
;	void super_put_tiny_small( int x, int y, int num ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	★縦ドット数nは、偶数でなければいけません
;	あらかじめ、super_convert_tiny でデータを変換しておく必要アリ
;	データ形式: (super_patdata[])
;		--------------------------
;		1ワード = (ひとつめの色コード << 8) + 80h
;		nバイト = データ
;		--------------------------
;		1ワード = (ふたつめの色コード << 8) + 80h
;		nバイト = データ
;		…
;		--------------------------
;		1ワード = 0 (終わり)
;
;	色が 4色より多いと、データが元より大きくなるので変換するとき注意
;
;	シューティングゲームなんかで、大量に表示される「弾」は、
;	8xn dot でできるよね?それならこれで高速にできるね
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 9/18 Initial: supertt8.asm / master.lib 0.21
;	94/ 8/16 [M0.23] 縦ドット数を可変に(ただし、偶数に限る)
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	super_patdata:WORD
	EXTRN	super_patsize:WORD

SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; 書き込みﾋﾞｯﾄが立っているﾄﾞｯﾄにﾀｲﾙﾚｼﾞｽﾀから書く

	.CODE

func SUPER_PUT_TINY_SMALL	; super_put_tiny_small() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg
	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+num]
	shl	BX,1
	mov	BP,super_patsize[BX]
	mov	DS,super_patdata[BX]

	mov	AX,DI
	shl	AX,2
	add	DI,AX
	shl	DI,4
	mov	AX,CX
	and	CX,7		; CL = x & 7
	shr	AX,3
	add	DI,AX		; DI = draw start offset

	xor	SI,SI

	lodsw
	cmp	AL,80h
	jne	short RETURN	; １色もないとき(おいおい)

	mov	AL,GC_RMW
	pushf
	CLI
	out	GC_MODEREG,AL
	popf

	push	AX
	mov	DX,BP			; DL=PATTERN_HEIGHT
	mov	AL,SCREEN_XBYTES
	mul	DL
	mov	BP,AX			; BP=PATTERN_HEIGHT * SCREEN_XBYTES
	shr	DL,1			; 2行単位で描画するから
	pop	AX

	EVEN
COLOR_LOOP:
	REPT 4
	shr	AH,1	; 色の指定あるね
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,DL
	EVEN
YLOOP:
	lodsw
	mov	BL,AH
	xor	AH,AH
	ror	AX,CL
	mov	ES:[DI],AX

	xor	BH,BH
	ror	BX,CL
	mov	ES:[DI+SCREEN_XBYTES],BX

	add	DI,SCREEN_XBYTES*2
	dec	CH
	jnz	short YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80H
	je	short COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off

RETURN:
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

END
