; master library - PC98
;
; Description:
;	RS-232C�����̂��߂̕ϐ���`
;
; Variables:
;	sio_send_buf	���M�o�b�t�@
;	sio_receive_buf	��M�o�b�t�@
;	sio_cmdbackup	�R�}���h���W�X�^�̌��݂̒l�̕ێ�
;	sio_send_rp	���M�o�b�t�@�ǂݍ��݃|�C���^
;	sio_send_wp	���M�o�b�t�@�������݃|�C���^
;	sio_SendLen	���M�o�b�t�@���̃f�[�^��
;	sio_receive_rp	��M�o�b�t�@�ǂݍ��݃|�C���^
;	sio_receive_wp	��M�o�b�t�@�������݃|�C���^
;	sio_ReceiveLen	��M�o�b�t�@���̃f�[�^��
;	sio_flow_type	�t���[����̕��@
;	sio_send_stop	���M�s�V����t���O
;	sio_rec_stop	��M���Q����t���O
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
;

	.MODEL SMALL

	include func.inc

DATADEF equ 1

	include sio.inc

	.DATA?

	public sio_send_buf
sio_send_buf	db	SENDBUF_SIZE dup (?)
	public sio_receive_buf
sio_receive_buf	db	RECEIVEBUF_SIZE dup (?)
	public sio_cmdbackup
sio_cmdbackup	dw	?	; �R�}���h���W�X�^�̃o�b�N�A�b�v


	public sio_send_rp
sio_send_rp	dw	?
	public sio_send_wp
sio_send_wp	dw	?
	public _sio_SendLen,sio_SendLen
_sio_SendLen label word
sio_SendLen	dw	?

	public sio_receive_rp
sio_receive_rp	dw	?
	public sio_receive_wp
sio_receive_wp	dw	?
	public _sio_ReceiveLen, sio_ReceiveLen
_sio_ReceiveLen label word
sio_ReceiveLen	dw	?

	public sio_flow_type
sio_flow_type	dw	?	; �t���[����̕���

	public sio_send_stop
sio_send_stop	dw	?	; ���M�t���[���� 0 = ���M��,1=��~, 2=��
	public sio_rec_stop
sio_rec_stop	dw	?	; ��M�t���[���� 0 = ��M��, 1 = ��~��
				;		 XON,XOFF = XON/XOFF�v��
				;  0�ɂ���͔̂񊄂荞��, 1�ɂ���̂͊��荞��


END
