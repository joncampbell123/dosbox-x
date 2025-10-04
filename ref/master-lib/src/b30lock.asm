; master library - MS-DOS - 30BIOS
;
; Description:
;	30�sBIOS����
;	�풓�����֎~�C����
;
; Procedures/Functions:
;	bios30_lock( void ) ;
;	bios30_unlock( void ) ;
;
; Parameters:
;	
;
; Returns:
;	bios30_lock:
;	bios30_unlock:
;		�ݒ��̏풓�����֎~���(���T�|�[�g�Ȃ��ɋU)
;		�U(0)  = �풓�������� / ���T�|�[�g
;		�^(!0) = �풓�����֎~
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30�sBIOS 1.20�ȍ~�ŗL��
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����`��
;
; Advice:
;	����������
;
; Revision History:
;	94/ 8/ 8  �����o�[�W�����B
;

	.MODEL SMALL
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_LOCK	; bios30_lock( void ) {
	_call	BIOS30_TT_EXIST		; 30�sBIOS 1.20�ȍ~
	and	al,08h
	jz	short LOCK_FAILURE

	mov	bl,02h			; �풓�����֎~
	mov	ax,0ff0bh
	int	18h
LOCK_FAILURE:
	ret
endfunc			; }


func BIOS30_UNLOCK	; bios30_unlock( void ) {
	_call	BIOS30_TT_EXIST		; 30�sBIOS 1.20�ȍ~
	and	al,08h
	jz	short UNLOCK_FAILURE

	mov	bl,00h			; �풓��������
	mov	ax,0ff0bh
	int	18h
UNLOCK_FAILURE:
	ret
endfunc			; }

	END
