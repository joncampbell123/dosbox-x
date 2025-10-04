; Master Library - XMS
;
; Description:
;	XMS�E���C���������Ԃ���ѓ��m�̃���������
;
; Functions:
;	int xms_movememory( long destOff, unsigned destHandle, long srcOff, unsigned srcHandle, long Length )
;
; Parameters:
;	long destOff		�]����̃A�h���X
;	unsigned destHandle	�]����̃n���h��(0=���C��������)
;	long srcOff		�]�����̃A�h���X
;	unsigned srcHandle	�]�����̃n���h��(0=���C��������)
;	long Length		�]������o�C�g��
;
; Returns:
;	none
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 80286
;
; Notes:
;	xms_exist�ő��݂ƕԂ���Ă��Ȃ���ԂŎ��s����Ɩ\�����܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/23 Initial
;	93/ 7/16 [M0.20] _xms_ControlFunc -> xms_ControlFunc�ɉ���
;	94/ 9/15 [M0.23] LARGECODE BUGFIX, ���ł�

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_MOVEMEMORY
	mov	BX,SP
	push	SI
	push	DS

	mov	AX,DS
	mov	ES,AX
	mov	AX,SS
	mov	DS,AX
	lea	SI,[BX+(RETSIZE)*2]	; length
	mov	AH,0bh
	call	ES:[xms_ControlFunc]
	pop	DS
	pop	SI
	ret 16
endfunc

END
