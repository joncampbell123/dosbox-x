; master library - PC-9801 - RS-232C - READ SIGNAL
;
; Description:
;	RS-232C�̊e���Ԃ𓾂�
;
; Function/Procedures:
;	int sio_read_signal( int ch ) ;
;	int sio_read_err( int ch ) ;
;	int sio_read_dr( int ch ) ;
;
; Parameters:
;	ch	RS-232C�|�[�g�̃`�����l���ԍ�( 0=�{�̓��� )
;
; Returns:
;	sio_read_signal	SIO_CI(80h) ���Č��o
;	sio_read_signal SIO_CS(40h) ���M��
;	sio_read_signal SIO_CD(20h) �L�����A���o
;	sio_read_err	SIO_PERR    �p���e�B�G���[
;	sio_read_err	SIO_OERR    �I�[�o�[�����G���[
;	sio_read_err	SIO_FERR    �t���[�~���O�G���[
;	sio_read_dr	!=0         DTR���o
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
; Assembly Language Note:
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
;	93/ 7/23 Initial: siosignal.asm/master.lib 0.20
;	93/ 9/22 [M0.21] sio_read_signal�̃r�b�g���]�Y��C��

	.MODEL SMALL
	include func.inc

	.CODE

func SIO_READ_SIGNAL	; sio_read_signal() {
	in	AL,33h
	not	AX
	and	AX,0e0h
	ret	2
endfunc		; }

func SIO_READ_ERR	; sio_read_err() {
	in	AL,32h
	and	AX,38h
	ret	2
endfunc		; }

func SIO_READ_DR	; sio_read_dr() {
	in	AL,32h
	and	AX,80h
	ret	2
endfunc		; }

END
