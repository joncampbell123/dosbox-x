; master library - MS-DOS
;
; Description:
;	�t�@�C���̓ǂݍ��݃I�[�v��
;
; Function/Procedures:
;	int file_ropen( const char * filename ) ;
;
; Parameters:
;	char * filename	�t�@�C����
;
; Returns:
;	1 = ����
;	0 = ���s ( ���łɉ������J����Ă���A������Ȃ� )
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
;	92/12/ 1
;	93/ 5/ 9 [M0.16] Pascal�Ή�
;	93/ 5/14 [M0.17] ������ő�n�O���������̂ŏC��

	.MODEL SMALL
	include func.inc
	EXTRN DOS_ROPEN:CALLMODEL

	.DATA
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:DWORD
	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE
func FILE_ROPEN
	push	BP
	mov	BP,SP

	; ����
	filename = (RETSIZE+1)*2

	xor	AX,AX
	mov	BX,file_Handle
	cmp	BX,-1			; ���ɉ����J���Ă�����ʖ�
	jne	short EXIT

	_push	[BP+filename+2]
	push	[BP+filename]
	call	DOS_ROPEN		; �t�@�C���̓ǂݍ��݃I�[�v��
	sbb	BX,BX
	or	AX,BX			; �G���[�Ȃ� -1
	mov	file_Handle,AX
	xor	AX,AX
	mov	file_InReadBuf,AX
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,AX
	mov	file_BufPtr,AX
	mov	file_Eof,AX
	mov	file_ErrorStat,AX
	lea	AX,[BX+1]		; OK = 1, FAULT = 0
EXIT:
	pop	BP
	ret	DATASIZE*2
	EVEN
endfunc
END

