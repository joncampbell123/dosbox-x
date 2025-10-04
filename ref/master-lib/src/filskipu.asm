; master library - MS-DOS
;
; Description:
;	�w��̕����܂œǂݎ̂Ă�
;
; Function/Procedures:
;	void file_skip_until( int stop ) ;
;
; Parameters:
;	int stop	��~����(1 byte)
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
;	��~�����܂ŁA�ǂݎ̂Ă܂��B
;	���������ꍇ�A���ɓǂݍ��߂�̂͒�~�����̎��̕����ł��B
;	������Ȃ������ꍇ�Afile_Eof = 1 �ɂȂ��Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc
	EXTRN FILE_GETC:CALLMODEL

	.DATA
	EXTRN file_Buffer:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD

	.CODE
func FILE_SKIP_UNTIL
	push	BP
	mov	BP,SP

	push	DI

	; ����
	stop	= (RETSIZE+1)*2

	cmp	WORD PTR file_Eof,0
	jne	short EXIT
SLOOP:
	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	jbe	short EMPTY

	mov	CX,file_InReadBuf
	sub	CX,AX
	les	DI,file_Buffer
	add	DI,AX
	mov	AX,[BP+stop]
	repne	scasb			; �{������
	je	short FOUND		; �݂�����

EMPTY:
	mov	AX,file_InReadBuf
	mov	file_BufPtr,AX
	call	FILE_GETC
	cmp	file_Eof,0
	jne	short EXIT

	cmp	AL,[BP+stop]
	jne	short SLOOP
	; �݂�����
	jmp	short EXIT
FOUND:
	sub	DI,WORD PTR file_Buffer ; ������������offset
	mov	file_BufPtr,DI
EXIT:
	pop	DI

	mov	SP,BP
	pop	BP
	ret	2
	EVEN
endfunc

END
