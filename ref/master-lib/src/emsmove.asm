; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMS�������Ǝ僁�����Ȃǂ̊ԂŃf�[�^��]������
;
; Function/Procedures:
;	int ems_movememoryregion( struct const EMS_move_source_dest * block ) ;
;
; Parameters:
;	struct .. * block �p�����[�^�u���b�N
;
; Returns:
;	0 ........... success
;	80h�` ....... failure(EMS �G���[�R�[�h)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 4.0
;
; Notes:
;	�ENEC �� EMS�h���C�o�́A���̃t�@���N�V�����ɂ���� segment B000h��
;	�@�������� VRAM�łȂ��ݒ肵�Ă��܂��܂��B
;	�@���s��Aems_enablepageframe()�ɂ���� VRAM�ɖ߂��ĉ������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial


	.MODEL SMALL
	include func.inc
	.CODE

func	EMS_MOVEMEMORYREGION
	push	SI
	mov	SI,SP

	_push	DS
	_lds	SI,SS:[SI+(RETSIZE+1)*2]
	mov	AX,5700h
	int	67h
	mov	AL,AH
	xor	AH,AH

	_pop	DS
	pop	SI
	ret	DATASIZE*2
endfunc

END
