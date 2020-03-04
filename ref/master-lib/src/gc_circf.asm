PAGE 98,120
; graphics - grcg - circlefill - PC98V
;
; DESCRIPTION:
;	円塗りつぶし (level 2)
;
; FUNCTION:
;	void far _pascal grcg_circlefill( int x, int y, unsigned r ) ;

; PARAMETERS:
;	int x		x座標
;	int y		y座標
;	unsigned r	半径
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
;	・描画規則はgrcg_hline, grcg_psetに依存しています。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリッピングに対応しています。
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;	grcg_hline(), grcg_pset()
;
; HISTORY:
;	93/ 3/ 1 Initial

	.186
	.MODEL SMALL
	.CODE
	include func.inc
		EXTRN	GRCG_HLINE:CALLMODEL
		EXTRN	GRCG_PSET:CALLMODEL

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	6
	endm

func GRCG_CIRCLEFILL	; {
	enter	2,0
	push	SI
	push	DI
	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	r	= (RETSIZE+1)*2

	; 変数
	s	= -2

	mov	AX,[BP+r]
	test	AX,AX
	jz	short	PSET

	; wx = SI
	; wy = DI
	mov	SI,AX
	mov	[BP+s],AX
	mov	DI,0
LOOP_8:
	mov	BX,[BP+x]
	mov	AX,BX
	sub	AX,SI
	push	AX	; x - wx
	add	BX,SI
	push	BX	; x + wx
	mov	AX,[BP+y]
	sub	AX,DI	; y - wy
	push	AX
	call	GRCG_HLINE

	mov	BX,[BP+x]
	mov	AX,BX
	sub	AX,SI
	push	AX	; x - wx
	add	BX,SI
	push	BX	; x + wx
	mov	AX,[BP+y]
	add	AX,DI	; y + wy
	push	AX
	call	GRCG_HLINE

	mov	AX,DI
	stc
	rcl	AX,1	; (wy << 1) + 1
	sub	[BP+s],AX
	jns	short LOOP_8E

	mov	BX,[BP+x]
	mov	AX,BX
	sub	AX,DI
	push	AX	; x - wy
	add	BX,DI
	push	BX	; x + wy
	mov	AX,[BP+y]
	sub	AX,SI	; y - wx
	push	AX
	call	GRCG_HLINE

	mov	BX,[BP+x]
	mov	AX,BX
	sub	AX,DI
	push	AX	; x - wy
	add	BX,DI
	push	BX	; x + wy
	mov	AX,[BP+y]
	add	AX,SI	; y + wx
	push	AX
	call	GRCG_HLINE

	dec	SI		; s += --wx << 1 ;
	mov	AX,SI
	shl	AX,1
	add	[BP+s],AX

LOOP_8E:
	inc	DI
	cmp	SI,DI
	jae	short	LOOP_8

	MRETURN

PSET:
	push	[BP+x]
	push	[BP+y]
	call	GRCG_PSET

	MRETURN
endfunc			; }

END
