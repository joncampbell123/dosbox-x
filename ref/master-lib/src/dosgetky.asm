; master library - MS-DOS
;
; Description:
;	DOS�E�W�����͂���̕����ǂݎ��Q
;	(^C�Ŏ~�܂�Ȃ�, ���͂��Ȃ��Ă��҂��Ȃ�)
;
; Function/Procedures:
;	int dos_getkey( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	1 .. 255 ���͂��ꂽ����
;		0 ...... ���͂Ȃ�
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
;	�L�����N�^�R�[�h 0 �̕����͓��͂Ȃ��Ƌ�ʂł��܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/20 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETKEY
	mov	AH,6
	mov	DL,0FFh
	int	21h
	xor	AH,AH
	ret
endfunc

END
