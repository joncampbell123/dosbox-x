; master library - PC98
;
; Description:
;	���M�o�b�t�@�ɂP�����ǉ�
;
; Function/Procedures:
;	int sio_putc( int port, int c ) ;
;
; Parameters:
;	port = ���M��|�[�g�ԍ�
;	c = ���M�������P�o�C�g�̃f�[�^
;
; Returns:
;	int	1 = ����
;		0 = �o�b�t�@�����t
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
;	93/ 7/23 [M0.20] ������port�𑫂�������
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

func SIO_PUTC
	push	BP
	mov	BP,SP

	; ����
	port	= (RETSIZE+2)*2
	c	= (RETSIZE+1)*2

	mov	CX,[BP+c]

	xor	AX,AX
	cmp	sio_SendLen,SENDBUF_SIZE
	je	short PUTC_FULL		; ���M�o�b�t�@�����t�Ȃ玸�s

	mov	BX,sio_send_wp
	mov	[BX+sio_send_buf],CL	; ���M�o�b�t�@�ɒǉ�
	RING_INC BX,SENDBUF_SIZE,AX
	mov	sio_send_wp,BX		; �o�b�t�@�̃|�C���^���X�V����
	inc	sio_SendLen

	pushf
	CLI
	cmp	sio_send_stop,1		; �~�߂��ĂȂ��Ȃ�A
	jle	short PUTC_DONE
	mov	AL,SIO_TXRE*2+1		; ���M���荞�݋���
	out	SYSTEM_CTRL,AL
	mov	sio_send_stop,0

PUTC_DONE:
	popf
	mov	AX,1
PUTC_FULL:
	pop	BP
	ret	4
endfunc

END
