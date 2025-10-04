; master library - MS-DOS
;
; Description:
;	��ƃo�b�t�@�̃t���b�V��
;	�t�@�C���̃N���[�Y
;
; Function/Procedures:
;	void file_flush( void ) ;
;	void file_close( void ) ;
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
;	���ˏ��F
;
; Revision History:
;	92/11/17 Initial
;	92/11/29 file_flush�ǉ�
;	94/ 2/10 [M0.22a] �������݃I�[�v����, file_BufferPos���X�V���ĂȂ�����
;	95/ 4/14 [M0.22k] BUGFIX read���file_flush()��A�܂�Ɍ��݈ʒu�����ꂽ

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD		; fil.asm
	EXTRN file_BufferSize:WORD	; fil.asm
	EXTRN file_BufferPos:DWORD	; fil.asm

	EXTRN file_BufPtr:WORD		; fil.asm
	EXTRN file_InReadBuf:WORD	; fil.asm
	EXTRN file_Eof:WORD		; fil.asm
	EXTRN file_ErrorStat:WORD	; fil.asm
	EXTRN file_Handle:WORD		; fil.asm

	.CODE

; out: BX = file handle
func FILE_FLUSH		; file_flush() {
	mov	BX,file_Handle
	cmp	BX,-1
	je	short F_IGNORE		; house keeping

	mov	AX,file_BufPtr		; opened for read? write?
	cmp	file_InReadBuf,AX	; InReadBuf < BufPtr then write
	jae	short F_READ

	; �������݃I�[�v������Ă���
	push	DS
	mov	CX,file_BufPtr
	lds	DX,file_Buffer
	mov	AH,40h			; �t�@�C���̏�������
	int	21h
	pop	DS
	jc	short ERROR
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0
	cmp	file_BufPtr,AX
	je	short WDONE
ERROR:
	mov	file_ErrorStat,1
WDONE:
	mov	file_BufPtr,0
	ret

F_READ:
	cmp	file_InReadBuf,0
	je	short F_IGNORE

	mov	DX,AX		; AX: file_BufPtr
	mov	CX,0
	add	DX,WORD PTR file_BufferPos
	mov	file_InReadBuf,CX
	mov	file_BufPtr,CX
	adc	CX,WORD PTR file_BufferPos+2
	mov	AX,4200h
	mov	BX,file_Handle	; �_���ʒu�ɕ����ʒu�����킹��
	int	21h		; seek

	; �_���ʒu�̐擪���X�V
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,DX
F_IGNORE:
	ret
endfunc			; }

func FILE_CLOSE		; file_close() {
	call	FILE_FLUSH		; �t���b�V��  BX��file handle���c

	mov	AH,3Eh			; �t�@�C���̃N���[�Y
	int	21h

	mov	file_Handle,-1		; �t�@�C���n���h���̖�����
IGNORE:
	ret
endfunc			; }

END
