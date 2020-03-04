; master library - 
;
; Description:
;	EGCによる部分下スクロール
;
; Functions/Procedures:
;	void egc_shift_down(int x1, int y1, int x2, int y2, int dots);
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
;$Id: dshift.asm 0.02 92/05/28 22:49:49 Kazumi Rel $
;	94/ 5/16 Initial: egcsftd.asm/master.lib 0.23
;
;
;
	.186

	.MODEL	SMALL

	include	func.inc

	.CODE
func EGC_SHIFT_DOWN	; egc_shift_down() {
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
	shl	AX,2
	add	AX,DI
	shl	AX,4			; dots * 80
	mov	WORD PTR CS:[diff],AX
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
	and	DL,0fh
	mov	DH,15
	sub	DH,DL			; Src Bit Address
	mov	DL,DH
	xor	DH,DH
	mov	AX,DX			; Dest Bit Address
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
	mov	AX,DI			; dir/Bitアドレス
	or	AX,1000h		; dir = 1
	out	DX,AX
	mov	DX,04aeh
	mov	AX,BP			; ビット長
	out	DX,AX
	std
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	DI,SI
	add	DI,1111h		; dummy
diff	EQU	$-2
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
	out	6ah,AL
	mov	AL,00h			; GRCG stop
	out	7ch,AL
	mov	AL,00000110b		; RegWriteDisable
	out	6ah,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	5*2
endfunc		; }
END
