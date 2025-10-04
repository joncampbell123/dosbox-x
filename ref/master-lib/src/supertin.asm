; superimpose & master library module
;
; Description:
;	16色パターンを 4色以内データに変換する(super_put_tiny用)
;
; Function/Procedures:
;	int super_convert_tiny( int num ) ;
;
; Parameters:
;	num	パターン番号
;
; Returns:
;	InvalidData	(cy=1) num が登録されたパターンではない
;	InvalidData	(cy=1) 色が5色以上ある(パターンは不変)
;	NoError		(cy=0) 成功           (パターンは変換された)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	以後、このパターン番号は super_putなどの16色系で表示しても
;	ぐちゃぐちゃになってますよ。
;	パターンの大きさはチェックしてまへん。
;	super_put_tinyは 16xnパターン専用, 
;	super_put_tiny_small 8xn専用なので注意しといてや
;	マスクパターンはしっかり対応してまーす
;
; Assembly Language Note:
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
;	93/ 9/18 Initial: supertin.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patdata:WORD
	EXTRN	super_patsize:WORD
	EXTRN	super_buffer:WORD
	EXTRN	super_patnum:WORD

	.CODE

MAX_COLOR equ 4

func SUPER_CONVERT_TINY ; super_convert_tiny() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	xor	DI,DI
	mov	AX,MAX_COLOR
	push	AX		; stack top = カウンタ

	; 引数
	num	= (RETSIZE+1)*2
	mov	BX,[BP+num]
	cmp	BX,super_patnum		; パターン番号異常
	jae	short NO_PATTERN
	shl	BX,1

	mov	CX,super_patdata[BX]
	jcxz	short NO_PATTERN	; パターン番号異常

	mov	AX,super_patsize[BX]
	mul	AH
	mov	BP,AX		; BP = patsize

	mov	ES,super_buffer

	mov	DS,CX
	mov	BH,0ffh		; bh = color

COLORLOOP:
	xor	SI,SI
	mov	AX,BX
	not	AX
	mov	AL,80h
	and	AH,0fh
	stosw
	mov	CX,BP
	shr	CX,1
	rep	movsw		; mask
	sub	DI,BP

	mov	BL,4	; number of bit
	mov	CX,BP
	shr	CX,1

	EVEN
BITLOOP:
	ror	BH,1
	sbb	DX,DX	; ひっくりかえってるので, BHは 15->0に進んでるけど
			; テストは 0->15の順に行われてるわけ
			; なぜcmcを入れないのかというと、偶数バイトで
			; 命令をそろえるためね
	; in: CX = BP / 2
ANDLOOP:
	lodsw
	xor	AX,DX
	and	ES:[DI],AX
	inc	DI
	inc	DI
	loop	short ANDLOOP

	sub	DI,BP
	mov	CX,BP
	shr	CX,1
	dec	BL
	jnz	short BITLOOP

	lea	DX,[DI-2]
	xor	AX,AX
	repe	scasw
	mov	DI,DX
	jz	short NEXTCOLOR
	lea	DI,[DI+BP+2]
	pop	AX		; カウンタ
	dec	AX
	push	AX		; カウンタ
	js	short TOOMANY_COLORS
NEXTCOLOR:
	sub	BH,11H
	jnc	short COLORLOOP

	; 書き戻す
	mov	CX,DI
	shr	CX,1
	push	DS	; DS <-> ES
	push	ES
	pop	DS
	pop	ES
	xor	AX,AX
	mov	DI,AX
	mov	SI,AX
	rep	movsw
	stosw		; terminator

	clc
	mov	AX,NoError
	jmp	short RETURN

TOOMANY_COLORS:
NO_PATTERN:
	mov	AX,InvalidData
	stc
	;jmp	short RETURN

RETURN:
	pop	DI	; カウンタを捨てる

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	2
endfunc		; }

END
