; master library - font - gaiji
;
; Description:
;	外字から半角フォントパターンを得る
;
; Function/Procedures:
;	void font_entry_gaiji( void ) ;
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
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
;	93/ 4/ 7 Initial: fontentg.asm/master.lib 0.16
;	94/ 1/26 [M0.22] font_AnkSizeチェック
;

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN FONT_READ:CALLMODEL

	.DATA
	EXTRN font_AnkSeg:WORD
	EXTRN font_AnkSize:WORD

	.CODE

func FONT_ENTRY_GAIJI	; font_entry_gaiji() {
	cmp	font_AnkSize,0110h
	jne	short IGNORE

	push	BP
	mov	BP,SP
	sub	SP,32

	tempbuf = -32

	push	SI
	push	DI

	mov	SI,07620h
	CLD

CLOOP:
	push	SI
	_push	SS
	lea	AX,[BP+tempbuf]
	push	AX
	call	FONT_READ
	lea	AX,[SI-7620h]
	add	AX,font_AnkSeg
	mov	ES,AX
	mov	CX,8
	mov	DI,0
	push	SI
	push	DS
	push	SS
	pop	DS
	lea	SI,[BP+tempbuf]
	rep	movsw
	pop	DS
	pop	SI
	inc	SI
	cmp	SI,769fh
	jle	short	CLOOP

	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
IGNORE:
	ret
endfunc			; }

END
