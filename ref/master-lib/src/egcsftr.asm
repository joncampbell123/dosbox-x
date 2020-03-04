; master library - 
;
; Description:
;	EGCによる部分右スクロール
;
; Functions/Procedures:
;	void egc_shift_right(int x1, int y1, int x2, int y2, int dots);
;
; Parameters:
;	x1, y1	
;	x2, y2	
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
;$Id: rshift.asm 0.04 92/05/28 23:00:46 Kazumi Rel $
;	94/ 5/16 Initial: egcsftr.asm/master.lib 0.23
;
	.186

	.MODEL	SMALL

	include	func.inc

	.CODE
func EGC_SHIFT_RIGHT	; egc_shift_right() {
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
	mov	DX,[BP+x2]
	mov	SI,[BP+y2]
	mov	DI,[BP+dots]
	mov	AX,DI
	dec	AX			; 94/05/13 dots が 16 の倍数のとき
	shr	AX,4
	shl	AX,1
	mov	BYTE PTR CS:[start_di],AL
	mov	AX,SI
	sub	AX,[BP+y1]
	mov	BX,AX
	inc	BX			; line count
	mov	AX,SI
	shl	SI,2
	add	SI,AX
	shl	SI,4			; y2 * 80
	mov	AX,DX
	shr	AX,3
	and	AX,0fffeh		; LSB = 0
	add	SI,AX			; start address
	shr	AX,1
	mov	BP,CX
	shr	BP,4
	sub	AX,BP
	inc	AX
	mov	BP,DX
	sub	BP,CX			; bit length(not inc !!)
	mov	CX,AX			; 転送 word 数
	mov	AX,DX
	add	DX,DI
	and	DL,0fh
	mov	DH,15
	sub	DH,DL			; Dest Bit Address
	and	AL,0fh
	mov	AH,15
	sub	AH,AL			; Src Bit Address
	mov	AL,AH
	xor	AH,AH
	mov	DL,DH
	xor	DH,DH
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
	mov	DI,DX
	mov	AX,41
	sub	AX,CX
	shl	AX,1
	mov	BYTE PTR CS:[sub_si],AL
	mov	BYTE PTR CS:[sub_di],AL
	mov	AL,00000111b		; RegWriteEnable
	out	6AH,AL
	mov	AL,80h			; CGmode = 1
	out	7ch,AL
	mov	AL,05h			; ExBit = 1
	out	6AH,AL
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
	or	AX,1000h		; Dir = 1
	out	DX,AX
	mov	DX,04aeh
	mov	AX,BP			; ビット長
	out	DX,AX
	std
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	DI,SI
	inc	DI
	inc	DI
	add	DI,80			; dummy
start_di	EQU	$-1
	mov	DX,CX
	EVEN
next_line:	mov	CX,DX
	rep	movsw
	inc	SI
	inc	SI
	inc	DI
	inc	DI
	sub	SI,80			; dummy
sub_si	EQU	$-1
	sub	DI,80			; dummy
sub_di	EQU	$-1
	dec	BX
	jne	next_line
	cld
	mov	DX,04a0h
	mov	AX,0fff0h
	out	DX,AX
	mov	AL,04h			; ExBit = 0
	out	6AH,AL
	mov	AL,00h			; GRCG stop
	out	7ch,AL
	mov	AL,00000110b		; RegWriteDisable
	out	6AH,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	5*2
endfunc			; }
END
