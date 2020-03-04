; master library - PC-9801 - ASM
;
; Description:
;	サウンドボードのレジスタ設定／読み込み
;
; Function/Procedures:
;	
;
; Subroutines:
;	SOUND_I:near
;	SOUND_O:near
;
; Parameters:
;	SOUND_O:	BH = reg number
;			BL = value
;	SOUND_I:	BH = reg number
;	SOUND_JOY:	BL = port 0fhにセットするデータ
;
; Break Registers:
;	AL,DX, BH(SOUND_JOY)
;
; Returns:
;	SOUND_I:	AL = read value
;	SOUND_JOY:	AL = port 0eh から読み込んだ値の反転
;
; Binding Target:
;	asm
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	サウンドドライバとの競合を避けるため、割り込み禁止状態で呼び出して
;	下さい。
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
;	93/ 5/ 2 Initial:soundio.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN js_bexist:WORD

	.CODE
	public SOUND_I,SOUND_O

WAITREADY macro
	local LO
LO:
	in	AL,DX
	test	AL,80h
	jnz	short LO
	endm


SOUND_O	proc near
	mov	DX,188h

	WAITREADY
	mov	AL,BH
	out	DX,AL

	WAITREADY
	inc	DX
	inc	DX
	mov	AL,BL
	out	DX,AL
	ret
SOUND_O	endp

SOUND_I proc near
	mov	DX,188h

	WAITREADY
	mov	AL,BH
	out	DX,AL

	WAITREADY
	inc	DX
	inc	DX
	in	AL,DX
	ret
SOUND_I endp

END
