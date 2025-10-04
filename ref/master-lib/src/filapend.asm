; master library - MS-DOS
;
; Description:
;	�t�@�C���̒ǉ��I�[�v��
;
; Function/Procedures:
;	int file_append( const char * filename ) ;
;	function file_append( filename:string ) : integer ;
;
; Parameters:
;	char * filename		�t�@�C����
;
; Returns:
;	1 = ����
;	0 = ���s
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
;	�t�@�C�������݂��Ȃ���΍쐬���܂��B
;	�܂��A�o�b�t�@�����O����Ă��Ȃ���Γǂݏ������\�ł��B
;	�@�o�b�t�@�����O���Ă���ꍇ�A�ǂݍ��݂Ə������݂̓���̊Ԃɂ�
;	file_flush�܂���file_seek�����s����K�v������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/14 Initial
;	93/ 5/15 [M0.16] pascal�Ή�(dos_axdx�g�p)

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE
	EXTRN DOS_AXDX:CALLMODEL

func FILE_APPEND
	push	BP
	mov	BP,SP

	; ����
	filename = (RETSIZE+1)*2

	mov	AX,0			; FAILURE
	mov	BX,file_Handle
	cmp	BX,-1			; house keeping
	jne	short EXIT

	mov	AX,3d02h		; �t�@�C���ւ̓ǂݏ����I�[�v��
	push	AX
	_push	[BP+filename+2]
	push	[BP+filename]
	_call	DOS_AXDX
	or	AX,DX			; �G���[�Ȃ� -1
	mov	file_Handle,AX
	mov	CX,AX
	xor	AX,AX
	mov	file_InReadBuf,AX
	mov	file_BufPtr,AX
	mov	file_Eof,AX
	mov	file_ErrorStat,AX
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,AX
	inc	DX
	jz	short EXIT

	; �����ֈړ�
	mov	BX,CX
	xor	CX,CX
	mov	DX,CX
	mov	AX,4202h	; seek from end
	int	21h
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,DX
	mov	AX,1

EXIT:	pop	BP
	ret	DATASIZE*2
endfunc

END
