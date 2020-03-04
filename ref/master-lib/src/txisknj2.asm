; master library - PC98
;
; Description:
;	テキスト画面の指定位置が漢字右半分かどうかを判定する
;
; Function/Procedures:
;	void text_iskanji2( int x, int y ) ;
;
; Parameters:
;	unsigned x	横座標 ( 0 ～ 79 )
;	unsigned y	縦座標 ( 0 ～ )
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
;	座標範囲のクリッピングはしてません。
;	常駐プログラムなどによって書かれた文字は誤判定をする可能性があります。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/15 Initial
;	93/ 1/17 SS!=DS対応
;	93/ 3/30 bugfix


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

; break: AX(retcode),BX,DX
func TEXT_ISKANJI2
	mov	BX,SP
	x	=	(RETSIZE+1)*2
	y	=	(RETSIZE+0)*2

	mov	AX,SS:[BX+y]
	mov	DX,AX
	shl	AX,1
	shl	AX,1
	add	AX,DX	; *5
	shl	AX,1
	add	AX,TextVramSeg	; AX = textvramseg + y * 10
	mov	BX,SS:[BX+x]
	shl	BX,1	; BX = x*2

	mov	ES,AX
	mov	AX,ES:[BX]
	or	AL,AH	;
	neg	AH	; if ( AH  &&  (AH | AL) & 0x80 )
	sbb	AH,AH	;     return 1 ;
	and	AH,AL	; else
	rol	AX,1	;     return 0 ;
	and	AX,1	;
	ret	4
endfunc

END
