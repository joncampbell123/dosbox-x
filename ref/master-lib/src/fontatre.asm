; master library - font - PC/AT
;
; Description:
;	AT互換機のBIOSを利用したフォント読み取り
;
; Function/Procedures:
;	int font_at_read( unsigned ccode, unsigned fontsize, void far * buf);
;
; Parameters:
;	ccode		文字コード
;	fontsize	上位8bit: 横ドット数
;			下位8bit: 縦ドット数
;	buf		フォントデータの格納先(ベタ形式)
;
; Returns:
;	NoError		成功
;	InvalidData	失敗
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT他
;
; Requiring Resources:
;	CPU: 8086
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
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 9 Initial: fontatre.asm/master.lib 0.22
;	94/ 7/27 [M0.23] font_AnkFunc/font_KanjiFunc, JIS漢字対応

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	Machine_State:WORD
	PC_AT equ 010h

	EXTRN	font_AnkFunc:DWORD
	EXTRN	font_KanjiFunc:DWORD
	EXTRN	font_AnkSeg:WORD

	.CODE

;	int font_at_read( unsigned ccode, unsigned fontsize, void far * buf);
func FONT_AT_READ	; font_at_read() {
	enter	32,0
	push	SI
	push	DI

	; 引数
	ccode	= (RETSIZE+4)*2
	fontsize = (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2
	tempbuf = -32

	test	Machine_State,PC_AT	; AT互換機じゃなきゃ嫌だ
	jz	SKIP

	mov	CX,[BP+ccode]
	mov	DX,[BP+fontsize]
	les	SI,[BP+buf]
	test	DX,DX
	jz	short MEMORY_READ

	or	CH,CH
	jge	short ANK_OR_SJIS

	; convert jis to shift-jis
	add	AX,0ff1fh
	shr	AH,1
	jnc	short skip0
	add	AL,5eh
skip0:
	cmp	AL,7fh
	sbb	AL,-1
	cmp	AH,2eh
	jbe	short skip1
	add	AH,40h
skip1:
	add	AH,71h

ANK_OR_SJIS:

	mov	AX,word ptr font_AnkFunc
	or	AX,word ptr font_AnkFunc+2
	jz	short BIOS

	test	CH,CH
	jnz	short KANJI
	cmp	DX,0810h
	jne	short BIOS
	call	dword ptr font_AnkFunc
	and	AX,0		; clc, NoError
	jmp	short DONE

KANJI:
	cmp	DX,1010h
	jne	short BIOS
	mov	AX,word ptr font_KanjiFunc
	or	AX,word ptr font_KanjiFunc+2
	jz	short BIOS
	call	dword ptr font_KanjiFunc
	and	AX,0		; clc, NoError
	jmp	short DONE

	; size指定が 0だった場合、98のfont_read相当の動作をする
MEMORY_READ:
	CLD
	test	CH,CH
	jz	short MEMORY_ANK
	push	CX		; code
	push	1010h		; 16x16dot
	push	SS
	lea	AX,[BP+tempbuf]
	push	AX
	_call	FONT_AT_READ		; call myself
	jc	short DONE
	lea	SI,[BP+tempbuf]
	les	DI,[BP+buf]
	mov	CX,16
MEMORY_LOOP:
	lodsw
	mov	ES:[DI+16],AH
	stosb
	loop	short MEMORY_LOOP
	jmp	short DONE

MEMORY_ANK:
	mov	DI,SI		; les	DI,[BP+buf]
	push	DS
	add	CX,font_AnkSeg
	mov	DS,CX
	mov	CX,16/2
	xor	SI,SI
	rep	movsw
	pop	DS
	jmp	short DONE

BIOS:
	mov	AX,1800h
	mov	BX,0
	int	10h
	mov	AH,0
	cmp	AL,0			; clc, NoError
	je	short DONE
SKIP:
	mov	AX,InvalidData
	stc
DONE:
	pop	DI
	pop	SI
	leave
	ret	4*2
endfunc			; }

END
