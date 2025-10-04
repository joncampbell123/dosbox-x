; master library - 
;
; Description:
;	EGCによる左スクロール
;
; Functions/Procedures:
;	void egc_shift_left_all(int dots);
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
;$Id: lshifta.asm 0.04 92/05/28 22:57:03 Kazumi Rel $
;	94/ 5/16 Initial: egcsftla.asm/master.lib 0.23
;

	.186
	.MODEL	SMALL

	include	func.inc

	.CODE
func EGC_SHIFT_LEFT_ALL	; egc_shift_left_all() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+1)*2

	mov	BX,[BP+x]
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
	mov	AX,BX			; dir/Bitアドレス
	and	AX,000fh
	out	DX,AX
	mov	DX,04aeh
	mov	AX,639
	sub	AX,BX			; ビット長
	out	DX,AX
	cld
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	SI,0000h
	mov	DI,0fffeh
	mov	AX,BX
	shr	BX,4
	mov	BP,41
	sub	BP,BX
	shl	BX,1
	dec	AX			; 94/05/13 dots が 16 の倍数のとき
	shr	AX,4
	shl	AX,1
	add	SI,AX
	mov	DX,2
	sub	DX,BX
	mov	BX,400
	EVEN
next_line:	mov	CX,BP
	rep	movsw
	sub	SI,DX
	sub	DI,DX
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
	ret	1*2
endfunc			; }
END
