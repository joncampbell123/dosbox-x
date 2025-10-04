; master library - 
;
; Description:
;	EGCによる円筒左スクロール
;
; Functions/Procedures:
;	void egc_scroll_left(int dots);
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
;$Id: lscroll.asm 0.06 92/05/28 22:53:14 Kazumi Rel $
;	94/ 5/16 Initial: egcscrll.asm/master.lib 0.23
;
	.MODEL	SMALL

	include	func.inc

	.DATA
word_buff	DW	4 dup (?)
word_mask	DW	00000h,00001h,00003h,00007h,0000fh,0001fh,0003fh,0007fh
		DW	000ffh,001ffh,003ffh,007ffh,00fffh,01fffh,03fffh,07fffh

	.CODE
func EGC_SCROLL_LEFT	; egc_scroll_left() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	x	= (RETSIZE+1)*2

	push	DS
	mov	BX,[BP+x]
	mov	DI,OFFSET word_buff
	mov	SI,0000h
	mov	AX,DS
	mov	ES,AX
	mov	CX,16
	sub	CX,BX
	mov	AX,0a800h
	mov	DS,AX
	mov	AX,[SI]
	ror	AX,CL
	stosw
	mov	AX,0b000h
	mov	DS,AX
	mov	AX,[SI]
	ror	AX,CL
	stosw
	mov	AX,0b800h
	mov	DS,AX
	mov	AX,[SI]
	ror	AX,CL
	stosw
	mov	AX,0e000h
	mov	DS,AX
	mov	AX,[SI]
	ror	AX,CL
	stosw
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
	mov	AX,BX			; Dir/Bitアドレス
	out	DX,AX
	mov	DX,04aeh
	mov	AX,639			; ビット長(640-16-4)-1(?)
	out	DX,AX
	cld
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	DX,400
	mov	SI,0000h
	mov	DI,0fffeh
	EVEN
next_line1:	mov	CX,41
	rep	movsw
	dec	SI
	dec	SI
	dec	DI
	dec	DI
	dec	DX
	jne	next_line1
	mov	DX,04ach
	mov	AX,1000h		; Dir/Bitアドレス
	out	DX,AX
	mov	DX,04aeh
	mov	AX,BX
	dec	AX			; ビット長(640-16-4)-1(?)
	test	BX,0fh
	jnz	short no_dec
	dec	AX
	EVEN
no_dec:	out	DX,AX			; 94/05/13 16 の倍数ドットのとき
	std
	mov	DX,400
	mov	SI,7cfeh
	mov	DI,7d00h+78
	EVEN
next_line2:	mov	CX,1
	rep	movsw
	sub	SI,78
	sub	DI,78
	dec	DX
	jne	next_line2
	cld
	mov	DX,04a0h
	mov	AX,0fff0h
	out	DX,AX
	mov	AL,04h			; ExBit = 0
	out	6ah,AL
	mov	AL,00000110b		; RegWriteDisable
	out	6ah,AL
	pop	DS
	mov	AL,0c0h			; RMW mode
	out	7ch,AL
	mov	CX,16
	sub	CX,BX
	mov	DI,78
	mov	SI,OFFSET word_mask
	shl	BX,1
	add	SI,BX
	mov	BX,[SI]
	mov	SI,OFFSET word_buff
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	mov	AL,BH
	stosb
	mov	SI,OFFSET word_buff
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	mov	AL,BL
	stosb
	mov	AL,00h			; GRCG stop
	out	7ch,AL

	pop	DI
	pop	SI
	pop	BP
	ret	1*2

endfunc			; }
END
