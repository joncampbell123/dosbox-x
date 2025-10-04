; master library - MS-DOS
;
; Description:
;	�t�@�C������̕�����̓ǂݍ���
;
; Function/Procedures:
;	unsigned file_gets( char far * buf, unsigned bsize, int endchar ) ;
;
; Parameters:
;	char far * buf	�ǂݍ��ݐ�
;	unsigned bsize	�ǂݎ���̗̈�̑傫��(�o�C�g��)
;	int endchar	��؂蕶��
;
; Returns:
;	�ǂ񂾕�����̒���
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
;	�E�t�@�C������Aendchar��ǂނ��Absize-1�o�C�g�ǂݎ��܂�
;	�@buf�ɓǂݍ��݂܂��B(endchar��ǂ񂾏ꍇ�Abuf�̖����Ɋ܂܂�܂�)
;	�@������ '\0'��ǉ����܂��B
;	�Ebuf �� NULL�������ꍇ�A�ő咷�w����� file_skip_until()�̂悤��
;	�@���������܂��B
;	�E�o�b�t�@�����O�����̎��͓��삵�܂���B
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
;	92/11/25 bugfix
;	94/ 2/21 [M0.22a] bugfix (�t�@�C���o�b�t�@�̖�����endchar�������Ƃ���
;			  �������Ă��܂��Ă���) �񍐂��񂫂�> iR
;			  buf��NULL�̂Ƃ����ς�����(x_x)

	.MODEL SMALL
	include func.inc
	EXTRN FILE_READ:CALLMODEL

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE
func FILE_GETS		; file_gets() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	buf	= (RETSIZE+3)*2
	bsize	= (RETSIZE+2)*2
	endchar	= (RETSIZE+1)*2

	CLD
	dec	WORD PTR [BP+bsize]
	mov	DI,[BP+bsize]		; DI = rest
RLOOP:
	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	jne	short ARU

	; �o�b�t�@���󂾂����ꍇ�A�ǂݍ��ށB
	sub	AX,AX
	push	AX
	push	AX
	push	AX
	_call	FILE_READ
ARU:
	mov	SI,file_InReadBuf
	sub	SI,file_BufPtr	; .InReadBuf - .BufPtr : �o�b�t�@�c��
	sub	SI,DI		; DI : �ǂ݂�������
	sbb	AX,AX
	and	SI,AX
	add	SI,DI
	; SI = len

	push	DI
	les	DI,file_Buffer
	add	DI,file_BufPtr
	mov	CX,SI
	mov	AL,[BP+endchar]
	repne	scasb		; endchar��{������̂���
	pop	DI
	pushf			; zflag��ۑ�
	mov	AX,SI
	sub	AX,CX

	push	[BP+buf+2]	; seg
	push	[BP+buf]	; off
	push	AX		; len
	_call	FILE_READ

	mov	BX,[BP+buf+2]
	or	BX,[BP+buf]
	jz	short NULL0
	add	[BP+buf],AX
NULL0:
	sub	DI,AX
	popf			; zflag���A
	jz	short EXITLOOP
	test	DI,DI
	jz	short EXITLOOP
	cmp	file_Eof,0
	je	short RLOOP
EXITLOOP:
	mov	AX,[BP+buf+2]
	or	AX,[BP+buf]
	jz	short NULL
	les	BX,[BP+buf]
	mov	BYTE PTR ES:[BX],0		; '\0'�𖖔��ɕt��
NULL:
	mov	AX,[BP+bsize]
	sub	AX,DI

	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	8
endfunc			; }

END
