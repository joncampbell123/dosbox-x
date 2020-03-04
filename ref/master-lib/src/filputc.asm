; master library - MS-DOS
;
; Description:
;	ÇPÉoÉCÉgÇÃèëÇ´çûÇ›
;
; Function/Procedures:
;	void file_putc( int chr ) ;
;
; Parameters:
;	none
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	óˆíÀè∫ïF
;
; Revision History:
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc
	EXTRN FILE_WRITE:CALLMODEL

	.DATA
	EXTRN file_Buffer:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD

	.CODE
func FILE_PUTC
	push	BP
	mov	BP,SP

	; à¯êî
	c	= (RETSIZE+1)*2

	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	jbe	short WFLASH

	les	BX,file_Buffer
	add	BX,AX		; file_BufPtrÇæÇÀ
	mov	CL,[BP+c]
	mov	ES:[BX],CL
	inc	AX
	mov	file_BufPtr,AX
	pop	BP
	ret	2
	EVEN
WFLASH:
	lea	AX,[BP+c]	;c
	push	SS
	push	AX
	mov	AX,1		; 1 bytesÇæÇÀ
	push	AX
	call	FILE_WRITE
	pop	BP
	ret	2
endfunc

END
