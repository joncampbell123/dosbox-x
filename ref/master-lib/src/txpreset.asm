; master library
;
; Description:
;	テキストセミグラフィックの点消去
;
; Function/Procedures:
;	void text_preset( int x, int y ) ;
;
; Parameters:
;	x,y	点を消す座標( x=0～159, y=0～99 (25行の時) )
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
;	・クリッピングは横方向(0～159)と、上(0～)だけ行っています。
;	・文字が書かれた場所を指定すると、そこを空白(00h)にします。
;	・点を消すことによって完全に点がなくなった領域は、
;	　セミグラフ属性を落とします。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/20 Initial
;	93/ 3/23 bugfix


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PRESET	; text_preset() {
	push	BP
	mov	BP,SP
	; 引数
	x = (RETSIZE+2)*2
	y = (RETSIZE+1)*2
	mov	AX,[BP+y]
	mov	CX,AX
	and	CX,3
	xor	AX,CX
	js	short OUTRANGE	; yがマイナスなら範囲外
	mov	BX,AX
	shr	AX,1
	shr	AX,1
	add	AX,BX
	shl	AX,1
	add	AX,TextVramSeg
	mov	ES,AX

	mov	BX,[BP+x]
	cmp	BX,160
	jae	short OUTRANGE	; xが 0～159以外なら範囲外

	shr	BX,1
	sbb	CH,CH
	and	CH,4
	or	CL,CH
	shl	BX,1
	mov	AX,0feh
	rol	AL,CL
	test	byte ptr ES:[BX+2000h],10h
	jz	short NOTCEMI
	and	ES:[BX],AX
	jz	short NOMOREDOT
OUTRANGE:
	pop	BP
	ret	4

	EVEN
NOMOREDOT:	; ドットが無くなったのならセミグラフ属性も解除
	and	byte ptr ES:[BX+2000h],not 10h
	pop	BP
	ret	4

	EVEN
NOTCEMI:	; 文字が表示されていたのならそれを消す
	mov	word ptr ES:[BX],0
	pop	BP
	ret	4
endfunc

END
