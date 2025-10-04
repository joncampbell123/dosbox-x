; master library - PC98
;
; Description:
;	���M�o�b�t�@�Ƀf�[�^����������
;	
;
; Function/Procedures:
;	unsigned sio_write( int ch, const void * senddata, unsigned sendlen ) ;
;
; Parameters:
;	ch	 ���M�|�[�g(0=�{�̓���)
;	senddata �������ރf�[�^�̐擪�A�h���X
;	sendlen	 �������ރf�[�^�̒���(�o�C�g��)
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
;	92/12/07 Initial
;	93/ 3/ 8 large data model bugfix
;	93/ 7/23 [M0.20] ch����������
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

func SIO_WRITE
	push	BP
	mov	BP,SP
	; ����
	channel = (RETSIZE+2+DATASIZE)*2
	senddata= (RETSIZE+2)*2
	sendlen	= (RETSIZE+1)*2

	mov	CX,[BP+sendlen]

	mov	AX,SENDBUF_SIZE	; ���M�o�b�t�@�̎c��o�C�g��
	sub	AX,sio_SendLen

	sub	AX,CX	; AX = min(AX,CX) (�������ޒ������Z�o)
	sbb	DX,DX
	and	AX,DX
	add	AX,CX
	jz	short WRITE_ZERO	; �������܂Ȃ��Ȃ�I��

	push	SI
	push	DI
	mov	BX,DS
	mov	ES,BX

	mov	DX,AX			; AX=DX �ɒ���������

	mov	BX,sio_send_wp
	lea	DI,[BX+sio_send_buf]
	mov	CX,SENDBUF_SIZE
	sub	CX,BX			; CX = �o�b�t�@�̒����̎c��

	sub	CX,DX	; CX = min(CX,DX)
	sbb	BX,BX	;(�P��ڂ̏������݂̒������Z�o)
	and	CX,BX
	add	CX,DX

	_push	DS
	_lds	SI,[BP+senddata]
	sub	DX,CX			; DX = �c�蒷��
	rep	movsb			; �P��ړ]��
	jz	short WRITE_SKIP2
	mov	CX,DX
	mov	DI,offset sio_send_buf
	rep	movsb			; �Q��ڂ̓]��
WRITE_SKIP2:
	_pop	DS
	add	sio_SendLen,AX

	sub	DI,offset sio_send_buf
	cmp	DI,SENDBUF_SIZE		; �E�[(���Ăǂ�)�̕␳
	sbb	DX,DX
	and	DI,DX
	mov	sio_send_wp,DI		; �o�b�t�@�̃|�C���^���X�V

	pushf
	CLI
	cmp	sio_send_stop,1		; �~�߂��ĂȂ��Ȃ�A
	jle	short WRITE_DONE
	mov	DX,AX
	mov	AL,SIO_TXRE*2+1		; ���M���荞�݋���
	out	SYSTEM_CTRL,AL
	mov	sio_send_stop,0
	mov	AX,DX
WRITE_DONE:
	popf
	pop	DI
	pop	SI

WRITE_ZERO:
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
