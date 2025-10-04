; master library - PC98
;
; Description:
;	��M�o�b�t�@����f�[�^��ǂݍ���
;
; Function/Procedures:
;	int sio_getc( int port ) ;
;	unsigned sio_read( int port, void * recbuf, unsigned reclen ) ;
;
; Parameters:
;	
;
; Returns:
;	sio_getc = 0�`255: �ǂݍ��񂾂P�o�C�g�̃f�[�^
;		   -1:	��M�o�b�t�@����
;	sio_read = �ǂݍ��񂾃o�C�g��( 0 = ��M�o�b�t�@�� )
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
;	92/12/07 Initial
;	93/ 2/19 request_xon()�̊��荞�݋֎~��Ԃ����炵��
;	93/ 7/23 [M0.20] ����port�ǉ��B���g�͂܂��Ȃ��B
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_BIT_ON:CALLMODEL


; ��M�o�b�t�@�ɗ����Ă���f�[�^�����ȉ��Ɍ������Ƃ��ɁA
; �ʐM����̑��M�����o��
; IN:
;	BX = port number ( 0=internal )
REQUEST_XON proc near
	cmp	sio_rec_stop,0
	je	short REQXON_NG
REQXON_OK:
	ret

REQXON_NG:
	; ��~���ŁA�o�b�t�@�����ȉ��Ɍ������Ȃ�΂ނ����ɑ��M�����c
	cmp	sio_ReceiveLen,FLOW_STOP_SIZE
	jae	short REQXON_OK		; �܂���������Ȃɂ����Ȃ���

	; �ނ����֑��M�����o��
	cmp	sio_flow_type,1		; SIO_FLOW_HARD:1
	ja	short REQXON_SOFT	; SIO_FLOW_SOFT:2
	jb	short REQXON_OK		; SIO_FLOW_NO:0
REQXON_HARD:
	; �n�[�h�t���[(RS/CS)
	push	AX
	push	BX			; port
	mov	AX,SIO_CMD_RS		; RS�M����ON
	push	AX
	call	SIO_BIT_ON
	pop	AX
	mov	sio_rec_stop,0		; ���M����
	jmp	short REQXON_OK

REQXON_SOFT:
	; �\�t�g�t���[(XON/XOFF)
	CLI
	push	AX
	mov	sio_rec_stop,XON	; XON���M�v�����o��
	mov	AL,SIO_TXRE*2+1		; ���M���荞�݋���
	out	SYSTEM_CTRL,AL
	pop	AX
	STI
	ret
REQUEST_XON endp


func SIO_GETC
	mov	AX,-1
	cmp	sio_ReceiveLen,0
	je	short GETC_NOBUF		; ��M�o�b�t�@����Ȃ玸�s

	mov	BX,sio_receive_rp
	xor	AH,AH
	mov	AL,[BX+sio_receive_buf]		; ��M�o�b�t�@���� 1byte���
	RING_INC BX,RECEIVEBUF_SIZE,DX		; �o�b�t�@�̃|�C���^���X�V����
	mov	sio_receive_rp,BX
	dec	sio_ReceiveLen

	mov	BX,SP
	port = (RETSIZE)*2
	mov	BX,SS:[BX+port]
	call	REQUEST_XON
GETC_NOBUF:
	ret	2
endfunc


func SIO_READ
	push	BP
	mov	BP,SP
	; ����
	port = (RETSIZE+2+DATASIZE)*2
	recbuf	= (RETSIZE+2)*2
	reclen	= (RETSIZE+1)*2

	mov	CX,[BP+reclen]
	mov	AX,sio_ReceiveLen	; ��M�o�b�t�@�̃o�C�g��

	sub	AX,CX	; AX = min(AX,CX) (�ǂݍ��ޒ������Z�o)
	sbb	DX,DX
	and	AX,DX
	add	AX,CX
	jz	short READ_ZERO		; �O�o�C�g�Ȃ疳��

	push	SI
	push	DI
	mov	DX,AX			; AX=DX �ɒ���������

	mov	BX,sio_receive_rp
	lea	SI,[BX+sio_receive_buf]
	mov	CX,RECEIVEBUF_SIZE
	sub	CX,BX			; CX = �o�b�t�@�̒����̎c��

	sub	CX,DX	; CX = min(CX,DX)
	sbb	BX,BX	;(�P��ڂ̓ǂݍ��݂̒������Z�o)
	and	CX,BX
	add	CX,DX

	s_mov	BX,DS
	s_mov	ES,BX
	_les	DI,[BP+recbuf]
	sub	DX,CX			; DX = �c�蒷��
	rep	movsb			; �P��ړ]��
	jz	short READ_SKIP2
	mov	CX,DX
	mov	SI,offset sio_receive_buf
	rep	movsb
READ_SKIP2:
	sub	sio_ReceiveLen,AX

	sub	SI,offset sio_receive_buf
	cmp	SI,RECEIVEBUF_SIZE	; �E�[(���Ăǂ�)�̕␳
	sbb	DX,DX
	and	SI,DX
	mov	sio_receive_rp,SI	; �o�b�t�@�̃|�C���^���X�V

	mov	BX,[BP+port]
	call	REQUEST_XON
	pop	DI
	pop	SI

READ_ZERO:
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
