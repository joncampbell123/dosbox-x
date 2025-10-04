; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMS �h���C�o�̑��݊m�F
;
; Function/Procedures:
;	int ems_exist(void) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 ... ���݂��Ȃ�
;		1 ... ���݂���
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
;	���ׂĂ� EMS�֐������s����O�ɂ��Ȃ炸����ő��ݔ�����s�����ƁB
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

func EMS_EXIST
	push	SI
	push	DI
	push	DS

	xor	AX,AX
	mov	ES,AX
	mov	ES,ES:[67h*4+2]
	mov	DI,000ah
	mov	BX,CS
	mov	DS,BX
	mov	SI,offset emm_device_name
	mov	CX,4
	repe	cmpsw
	jne	short NAI
	inc	AX
NAI:
	pop	DS
	pop	DI
	pop	SI
	ret
endfunc

emm_device_name db 'EMMXXXX0'

END
