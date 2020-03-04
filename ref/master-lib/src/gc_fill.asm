PAGE 98,120
; graphics - grcg - fill - PC98V
;
; Function:
;	void far _pascal grcg_fill( void ) ;
;
; Description:
;	クリップ枠内を塗りつぶす(青プレーンのみ)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo-Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;	for ALL memory model(SMALL以外にするときは .MODELを書き換えて下さい)
;
; Notes:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリップ枠全面を塗りつぶします。
;
; 関連関数:
;	grc_setclip()
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/6/13 Initial
;	92/6/16 TASM対応

	.186
	.MODEL SMALL

	.DATA

	; grc_setclip()関連
	EXTRN	ClipXL: WORD, ClipXW: WORD
	EXTRN	ClipYT_seg: WORD, ClipYB_adr: WORD

	; エッジパターン
	EXTRN	EDGES: WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret
	EVEN
	endm

; void far _pascal grcg_fill( void ) ;
func GRCG_FILL
	push	BP
	push	SI
	push	DI

	mov	AX,ClipYT_seg	;
	mov	ES,AX		;

	mov	BX,ClipXL	; BX = x1(left)

	mov	DI,BX		; DI = + ClipYB_adr + xl / 16 * 2
	shr	DI,4		;
	shl	DI,1		;
	add	DI,ClipYB_adr	;

	and	BX,0Fh
	lea	SI,[BX-10h]	; SI = bitlen
	shl	BX,1
	mov	DX,[EDGES+BX]
	not	DX		; DX = first word
	add	SI,ClipXW
	mov	BX,SI
	and	BX,0Fh
	shl	BX,1
	mov	BX,[EDGES+2+BX]	; BX = last word

	sar	SI,4		; SI = num words
	js	short SHORTBOX

	; 現在のデータ (AX,CXはフリー)
	; DX	first word
	; BX	last word
	; SI	num words
	; DI	start address
	; BP	sub offset
	; ES	gseg...

; 横幅が２ワード以上あるとき ==============
	lea	BP,[2+40+SI]	; BP = (num words + 2 + 40) * 2
	shl	BP,1		;
	EVEN
YLOOP:
	mov	AX,DX
	stosw
	mov	AX,0FFFFh
	mov	CX,SI
	rep	stosw
	mov	AX,BX
	stosw
	sub	DI,BP
	jnb short YLOOP
	MRETURN

; 横幅が１ワードしかないとき ==============
SHORTBOX:
	mov	BP,82		; BP = 横幅+1ワード
	mov	AX,DX
	and	AX,BX
	EVEN
YLOOP2:
	stosw
	sub	DI,BP
	jnb short YLOOP2
	MRETURN
endfunc

END
