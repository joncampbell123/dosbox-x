; master library - DOS - COPY
;
; Description:
;	�t�@�C���̃R�s�[
;
; Function/Procedures:
;	int dos_copy( int src_fd, int dest_fd, unsigned long copy_len ) ;
;
; Parameters:
;	src_fd    �R�s�[���t�@�C���n���h��
;	dest_fd   �R�s�[��t�@�C���n���h��
;	copy_len  �R�s�[����o�C�g��( 0ffffffffh�Ȃ��EOF�܂� )
;
; Returns:
;	NoError			����
;
;	GeneralFailure		�ǂݍ��ݎ��s(������copy_len�ɖ����Ȃ�)
;	GeneralFailure		�������ݎ��s(�f�B�X�N�s��?)
;
;	InsufficientMemory	��ƃ������s��
;	AccessDenied		�ǂ��炩�̃t�@�C���n���h���̃A�N�Z�X����������
;	InvalidHandle		�ǂ��炩�̃t�@�C���n���h��������
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS 2.1 or later
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�@copy_len = 0ffffffffh �̏ꍇ�̂݁A�r���� srd_fd �� EOF �ɒB�����ꍇ
;	�ł� NoError ���Ԃ�܂��B
;
; Assembly Language Note:
;	NoError�̂Ƃ��� cy=0, ����ȊO�� cy=1�ɂȂ��Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 8/ 1 Initial: doscopy.asm/master.lib 0.20
;	93/ 8/26 [M0.21] BUGFIX �������̉�����ł��ĂȂ�����(^^;
;	93/ 9/14 [M0.21] �o�b�t�@�T�C�Y��32K���珇�� 2K�܂Ŕ��������Ē���
;	93/11/17 [M0.21] ����(;_;)�P���ȃ~�X�����B�T�C�Y�w�肵���玸�s���Ă�

	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
BUFSIZE equ 32768
MIN_BUFSIZE equ 2048

func DOS_COPY	; dos_copy() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	src_fd	= (RETSIZE+4)*2
	dest_fd	= (RETSIZE+3)*2
	copy_len = (RETSIZE+1)*2

	mov	CX,BUFSIZE
ALLOCLOOP:
	push	CX
	_call	SMEM_WGET
	jnc	short GO

	shr	CX,1
	cmp	CX,MIN_BUFSIZE
	jnc	short ALLOCLOOP
	jmp	short MEM_ERROR
GO:

	push	DS
	mov	DS,AX

	mov	DI,[BP+copy_len+2]
	mov	SI,[BP+copy_len]
L:
	test	DI,DI
	jnz	short GO_MAX
	cmp	CX,SI
	jbe	short GO_MAX
	mov	CX,SI
GO_MAX:
	xor	DX,DX
	mov	BX,[BP+src_fd]
	mov	AH,3fh		; read handle
	int	21h
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX
	jc	short ERROR
	cmp	AX,CX
	jz	short CONT

	mov	CX,AX
	mov	AX,[BP+copy_len]
	and	AX,[BP+copy_len+2]
	inc	AX
	mov	AX,GeneralFailure
	stc
	jnz	short ERROR

	xor	DI,DI
	mov	SI,CX

CONT:
	xor	DX,DX
	mov	BX,[BP+dest_fd]
	mov	AH,40h		; write handle
	int	21h
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX
	jc	short ERROR

	cmp	AX,CX
	mov	AX,GeneralFailure
	stc
	jne	short ERROR

	sub	SI,CX
	sbb	DI,0
	jnz	short L
	cmp	SI,DI		; 0
	jne	short L

	mov	AX,SI		; 0 (= NoError)

ERROR:
	mov	BX,DS
	pop	DS
	push	BX
	_call	SMEM_RELEASE
MEM_ERROR:
	pop	DI
	pop	SI
	pop	BP
	ret	8
endfunc		; }

END
