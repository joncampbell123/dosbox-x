; master library - 
;
; Description:
;	EGCによる部分左スクロール
;
; Functions/Procedures:
;	void egc_shift_left(int x1, int y1, int x2, int y2, int dots);
;
; Parameters:
;	dots
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801VX
;
; Requiring Resources:
;	CPU: 186
;	EGC
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi
;
; Revision History:
;$Id: lshift.asm 0.04 92/05/28 22:55:10 Kazumi Rel $
;	94/ 5/16 Initial: egcsftl.asm/master.lib 0.23
;

	.186
	.MODEL	SMALL

	include	func.inc

	.CODE
func EGC_SHIFT_LEFT	; egc_shift_left() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x1	= (RETSIZE+5)*2
	y1	= (RETSIZE+4)*2
	x2	= (RETSIZE+3)*2
	y2	= (RETSIZE+2)*2
	dots	= (RETSIZE+1)*2

	mov	CX,[BP+x1]
	mov	SI,[BP+y1]
	mov	DX,[BP+x2]
	mov	BX,[BP+y2]
	mov	DI,[BP+dots]
	mov	AX,DI
	dec	AX			; 94/05/13 dots が 16 の倍数のとき
	shr	AX,4
	shl	AX,1
	mov	BYTE PTR CS:[start_di],AL
	sub	BX,SI
	inc	BX			; line count
	mov	AX,SI
	shl	SI,2
	add	SI,AX
	shl	SI,4			; y1 * 80
	mov	AX,CX
	shr	AX,3
	and	AX,0fffeh		; LSB = 0
	add	SI,AX			; start address
	shr	AX,1
	mov	BP,DX
	shr	BP,4
	sub	BP,AX
	inc	BP
	mov	AX,BP
	mov	BP,DX
	sub	BP,CX			; bit length(not inc !!)
	mov	ES,CX
	mov	CX,AX			; 転送 word 数
	mov	DX,ES
	sub	DX,DI
	and	DX,000fh		; Dest Bit Address
	mov	AX,ES
	and	AX,000fh		; Src Bit Address
	push	BX
	push	BP
	mov	BX,BP
	add	BX,AX
	shr	BX,4
	add	BP,DX
	shr	BP,4
	cmp	AL,DL
	je	short none
	jl	short less
	cmp	BX,BP
	jne	short none
	jmp	SHORT plus
	EVEN
less:	cmp	BX,BP
	jge	short none
	EVEN
plus:	inc	CX
	EVEN
none:	pop	BP
	pop	BX
	shl	DL,4
	or	DL,AL
	xor	DH,DH
	mov	DI,DX
	mov	AX,41
	sub	AX,CX
	shl	AX,1
	mov	BYTE PTR CS:[add_si],AL
	mov	BYTE PTR CS:[add_di],AL
	mov	AL,00000111b		; RegWriteEnable
	out	6ah,AL
	mov	AL,80h			; CGmode = 1
	out	7ch,AL
	mov	AL,05h			; ExBit = 1
	out	6ah,AL
	mov	DX,04a0h
	mov	AX,0fff0h		; アクティブプレーン
	out	DX,AX
	mov	DX,04a2h
	mov	AX,00ffh		; FGC/BGC/リードプレーン
	out	DX,AX
	mov	DX,04a4h
	mov	AX,28f0h		; モードレジスタ/ROPコードレジスタ
	out	DX,AX
	mov	DX,04a8h
	mov	AX,0ffffh		; マスクレジスタ
	out	DX,AX
	mov	DX,04ach
	mov	AX,DI			; Dir/Bitアドレス
	out	DX,AX
	mov	DX,04aeh
	mov	AX,BP			; ビット長
	out	DX,AX
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	DI,SI
	dec	DI
	dec	DI
	sub	DI,80			; dummy
start_di	EQU	$-1
	mov	DX,CX
	EVEN
next_line:	mov	CX,DX
	rep	movsw
	dec	SI
	dec	SI
	dec	DI
	dec	DI
	add	SI,80			; dummy
add_si	EQU	$-1
	add	DI,80			; dummy
add_di	EQU	$-1
	dec	BX
	jne	next_line
	mov	DX,04a0h
	mov	AX,0fff0h
	out	DX,AX
	mov	AL,04h			; ExBit = 0
	out	6ah,AL
	mov	AL,00h			; GRCG stop
	out	7ch,AL
	mov	AL,00000110b		; RegWriteDIsable
	out	6ah,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	5*2
endfunc			; }
END
