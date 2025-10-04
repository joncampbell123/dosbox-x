; master library - PC98
;
; Description:
;	テキスト画面への外字の書き込み
;	属性つき
;
; Function/Procedures:
;	void gaiji_putca( unsigned x, unsigned y, unsigned chr, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	unsigned chr	文字( 0 ～ 255, 下位8bitのみ有効 )
;	unsigned atrb	属性
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial
;	93/ 1/26 bugfix


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func GAIJI_PUTCA
	mov	DX,BP	; push BP
	mov	BP,SP

	mov	CX,DI	; push DI

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	chr	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	mov	AX,[BP + y]	; アドレス計算
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1		; DI = y * 10
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,[BP + x]
	shl	DI,1

	mov	AH,[BP + chr]
	mov	AL,0
	rol	AX,1
	shr	AX,1
	adc	AX,56h		; 上下逆転に注意

	mov	BX,[BP+atrb]	; 属性

	mov	BP,DX	; pop BP

	mov	ES:[DI+2000h],BX
	stosw
	or	AH,80h
	mov	ES:[DI+2000h],BX
	stosw

	mov	DI,CX	; pop	DI
	ret	8
endfunc

END
