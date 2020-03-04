PAGE 98,120
; graphics - grcg - hline - PC98V
;
; DESCRIPTION:
;	水平線の描画(青プレーンのみ)
;
; FUNCTION:
;	void far _pascal grcg_hline( int xl, int xr, int y ) ;

; PARAMETERS:
;	int xl	左端のx座標
;	int xr	右端のx座標
;	int y	y座標
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
;
; HISTORY:
;	92/6/6	Initial
;	92/6/16 TASM対応
;	93/ 5/29 [M0.18] .CONST->.DATA

	.186

	.MODEL SMALL

	.DATA

	EXTRN	EDGES:WORD

	.DATA?

	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

; void far _pascal grcg_hline( int xl, int xr, int y ) ;
;
func GRCG_HLINE
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	xl = (RETSIZE+3)*2
	xr = (RETSIZE+2)*2
	 y = (RETSIZE+1)*2

	mov	DX,[BP+y]
	sub	DX,ClipYT	;
	cmp	DX,ClipYH	;
	ja	short RETURN	; y がクリップ範囲外 -> skip

	mov	CX,[BP+xl]
	mov	BX,[BP+xr]

	mov	BP,DX		; BP=VRAM ADDR
	shl	BP,2		;
	add	BP,DX		;
	shl	BP,4		;

	mov	AX,ClipXL	; クリップ枠左端分ずらす
	sub	CX,AX
	sub	BX,AX

	test	CX,BX		; xが共にマイナスなら範囲外
	js	short RETURN

	cmp	CX,BX
	jg	short SKIP
	xchg	CX,BX		; CXは必ず非負になる
SKIP:
	cmp	BX,8000h	; if ( BX < 0 ) BX = 0 ;
	sbb	DX,DX
	and	BX,DX

	mov	DI,ClipXW
	sub	CX,DI		; if ( CX >= ClipXW ) CX = ClipXW ;
	sbb	DX,DX
	and	CX,DX
	add	CX,DI

	sub	CX,BX			; CX = bitlen
					; BX = left-x
	jl	short RETURN	; ともに右に範囲外 → skip

	mov	ES,ClipYT_seg	; セグメント設定

	add	BX,AX		; (AX=ClipXL)
	mov	DI,BX		; addr = yaddr + xl / 16 * 2 ;
	shr	DI,4
	shl	DI,1
	add	DI,BP

	and	BX,0Fh		; BX = xl & 15
	add	CX,BX
	sub	CX,16
	shl	BX,1
	mov	AX,[EDGES+BX]	; 左エッジ
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short LAST
	stosw
	mov	AX,0FFFFh
	rep stosw
LAST:	and	AX,[EDGES+2+BX]	; 右エッジ
	stosw
RETURN:
	MRETURN
endfunc
END
