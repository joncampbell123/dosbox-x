; master library - PC-9801
;
; Description:
;	カーソル形状の設定2(AND,XORマスク型のものに少し結果を近づける)
;
; Function/Procedures:
;	void cursor_pattern2( int px, int py, int whc, void far * pattern ) ;
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
;	
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
;	93/12/22 Initial: curpat2.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc
	EXTRN CURSOR_PATTERN:CALLMODEL

	.CODE

func CURSOR_PATTERN2	; cursor_pattern2() {
	push	BP
	mov	BP,SP
	sub	SP,64
	push	SI
	push	DI

	; 引数
	px	= (RETSIZE+5)*2
	py	= (RETSIZE+4)*2
	whc	= (RETSIZE+3)*2
	pattern	= (RETSIZE+1)*2

	temppat = -64

	push	[BP+px]
	push	[BP+py]
	xor	AX,AX
	push	AX
	push	[BP+whc]
	push	SS
	lea	DI,[BP+temppat]
	push	DI


	push	SS
	pop	ES

	push	DS
	lds	SI,[BP+pattern]

	CLD

	mov	CX,16
pat_loop:
	lodsw
	not	AX
	stosw
	loop	pat_loop
	mov	CX,16
	rep	movsw
	pop	DS

	call	CURSOR_PATTERN
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	10
endfunc		; }

END
