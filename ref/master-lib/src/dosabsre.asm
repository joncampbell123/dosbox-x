; master library - MS-DOS
;
; Description:
;	
;
; Function/Procedures:
;	void dos_absread( int drive, void far * buf, int power, long sector ) ;
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
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
;	óˆíÀè∫ïF
;
; Revision History:
;	92/11/19 Initial
;	93/1/17 êVå`éÆåƒÇ—èoÇµÇDOS5Ç≈ÇÕÇ»Ç≠ DOS4Ç©ÇÁÇ…ÇµÇΩ

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN	__osmajor:WORD

	.CODE


func DOS_ABSREAD
	push	BP
	mov	BP,SP
	push	DS

	; à¯êî
	drive	= (RETSIZE+6)*2
	buf	= (RETSIZE+4)*2
	power	= (RETSIZE+3)*2
	sector	= (RETSIZE+1)*2

	mov	AX,[BP+drive]

	cmp	__osmajor,4
	jl	short DOS3
DOS5:	mov	CX,-1
	mov	BX,SS
	mov	DS,BX
	lea	BX,[BP+sector]	; param table...
	jmp	short ACCESS
DOS3:
	mov	DX,[BP+sector]
	mov	CX,[BP+power]
	lds	BX,[BP+buf]
ACCESS:
	push	SI
	push	DI
	int	25h
	popf
	cld
	pop	DI
	pop	SI

	pop	DS
	pop	BP
	ret	12
endfunc

END
