; master library - PC98
;
; Description:
;	テキスト画面への外字数字列の書き込み
;	幅指定・属性なし
;
; Function/Procedures:
;	procedure gaiji_putni( x,y,val,width_ : Word ) ;
;
; Parameters:
;	x	左端の座標 ( 0 ～ 79 )
;	y	上端の座標 ( 0 ～ 24 )
;	val	数値
;	width_	桁数( 1～ )
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
;	width_ = 0 だと何もしません。
;	クリッピングしてません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial: gjputs.asm
;	93/1/26 bugfix
;	93/8/17 Initial: gjputni.asm/master.lib 0.21

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func GAIJI_PUTNI	; gaiji_putni {
	mov	DX,BP	; push BP
	mov	BP,SP

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	val	= (RETSIZE+1)*2
	width_	= (RETSIZE+0)*2

	mov	CX,[BP+width_]
	jcxz	short IGNORE

	push	SI
	push	DI

	mov	AX,[BP + y]	; アドレス計算
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1		; DI = y * 10
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,CX
	shl	DI,1
	add	DI,[BP + x]	; DI = x*2 + cx*4 - 2
	dec	DI
	shl	DI,1

	mov	SI,[BP+val]
	mov	BP,DX	; pop BP

	mov	BX,10
	STD

	EVEN
SLOOP:
	xor	DX,DX
	mov	AX,SI
	div	BX
	mov	SI,AX

	mov	AH,DL
	add	AH,30H+80h
	mov	AL,56h
	stosw
	and	AH,7fh
	stosw

	loop	short SLOOP

	CLD
	pop	DI
	pop	SI

IGNORE:
	ret	4*2
endfunc			; }

END
