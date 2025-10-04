; master library - PC98 - RS-232C - PascalString
;
; Description:
;	���M�o�b�t�@�Ƀp�X�J�����������������
;
; Function/Procedures:
;	unsigned sio_puts( int port, char void * passtr ) ;
;
; Parameters:
;	port	���M����|�[�g(0=�{�̓���)
;	sendstr �������ރp�X�J��������
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
;	93/ 7/23 Initial: sioputp.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_WRITE:CALLMODEL

func SIO_PUTP	; sio_putp() {
	push	BP
	mov	BP,SP

	; ����
	port	= (RETSIZE+1+DATASIZE)*2
	sendstr	= (RETSIZE+1)*2

	push	[BP+port]
	_les	BX,[BP+sendstr]
	_push	ES	; parameter to sio_write
  s_	<mov	AL,[BX]>
  l_	<mov	AL,ES:[BX]>
  	mov	AH,0
	inc	BX
	push	BX
	push	AX
	call	SIO_WRITE

	pop	BP
	ret	(1+DATASIZE)*2
endfunc		; }

END
