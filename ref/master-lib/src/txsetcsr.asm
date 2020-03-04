; master library - PC98
;
; Description:
;	カーソル形状の設定
;
; Function/Procedures:
;	void text_setcursor( int csrtype );
;
; Parameters:
;	csrtype		0 = 大きなカーソル
;			1 = 下４分１のカーソル
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
;	92/11/15 Initial
;	93/ 1/16 SS!=DS対応
;	93/ 5/20 [M0.18] hireso 31line

	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN TextVramSeg:WORD
	.CODE


; ＧＤＣのバッファが空になるまで待つ
; 910422
; no registers will broken
wait_for_gdc_empty	proc
	push	AX
WLOOP:
	jmp	short $+2
	in	AL,060h
	test	AL,4	; empty bit
	je	short WLOOP
	pop	AX
	ret
wait_for_gdc_empty	endp


func TEXT_SETCURSOR
	cmp	TextVramSeg,0e000h
	jb	short NORMALMODE
HIRESOMODE:
	mov	BX,SP
	mov	BX,SS:[BX+RETSIZE*2]

	mov	DX,000eh	; solid box cursor
	and	BX,1
	je	short SKIP
		mov	DH,7	; half box cursor
SKIP:

	mov	AH,0bh
	int	18h
	test	AL,10h	; 31line?
	jne	short GO
	dec	DL	; 31line
	dec	DL
GO:
	mov	AH,1eh	; set cursor form
	mov	AL,11001100b
;		   !!!===== blink rate(default=01100b, 00001=fastest)
;		   !!+----- KCG access mode, 0=code, 1=bitmap
;		   !+------ Cursor display,  0=hide, 1=show
;		   +------- blink?,	     0=light,1=blink

	int	18h
	ret	2

NORMALMODE:
	call	wait_for_gdc_empty
	mov	BX,SP
	mov	BX,SS:[BX+RETSIZE*2]
	and	BX,00ffh
	je	short OK
		mov	BX,12
OK:	mov	AL,4bh		;
	out	062h, AL	; CSRFORM
	jmp	short $+2
	jmp	short $+2
	jmp	short $+2
	mov	AL,10001111b
	out	060h, AL
	jmp	short $+2
	jmp	short $+2
	jmp	short $+2
	mov	AL,BL
	out	060h, AL
	jmp	short $+2
	jmp	short $+2
	jmp	short $+2
	mov	AL, 01111011b
	out	060h, AL
	ret	2
endfunc

END
