; master library - PC98
;
; Description:
;	���M�o�b�t�@�ɕ��������������
;
; Function/Procedures:
;	unsigned sio_puts( int port, char void * sendstr ) ;
;
; Parameters:
;	port	���M����|�[�g(0=�{�̓���)
;	sendstr �������ޕ�����
;
; Returns:
;	�������񂾃o�C�g��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���M�o�b�t�@���c�菭�Ȃ��Ƃ��͂ł��邩����i�[���Asendlen���
;	���Ȃ��l��Ԃ��ꍇ������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/ 4 Initial
;	93/ 7/23 [M0.20] port�ǉ�
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_WRITE:CALLMODEL

func SIO_PUTS
	push	BP
	mov	BP,SP
	push	DI

	; ����
	port	= (RETSIZE+1+DATASIZE)*2
	sendstr	= (RETSIZE+1)*2

	push	[BP+port]
  s_	<mov ax,ds>
  s_	<mov es,ax>
	_les	DI,[BP+sendstr]
	_push	ES	; parameter to sio_write
	push	DI

	mov	CX,-1
	mov	AL,0
	repne	scasb
	not	CX
	dec	CX	; CX = len(sendstr)

	push	CX
	call	SIO_WRITE

	pop	DI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
