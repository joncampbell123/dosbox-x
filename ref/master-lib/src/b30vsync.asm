; master library - MS-DOS - 30BIOS
;
; Description:
;	30�sBIOS����
;	VSYNC���g���̎擾
;
; Procedures/Functions:
;	unsigned bios30_getvsync( void ) ;
;
; Parameters:
;	
;
; Returns:
;	bios30_getvsync:
;		���݂̎�����VSYNC���g��(���T�|�[�g||�v���s�\�Ȃ�0x0000)
;		���8�r�b�g�͐�����
;		����8�r�b�g�͏�����
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
;	���̊֐��Ŏg�p���Ă���30�sBIOS API ff0ah���Ăяo�����Ƃɂ��
;	�r�[�v�����g���������������
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����`��
;
; Advice:
;	KEI SAKAKI.
;
; Revision History:
;	94/ 8/ 5  �����o�[�W�����B
;

	.MODEL SMALL
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETVSYNC	; unsigned bios30_getvsync( void ) {
	_call	BIOS30_TT_EXIST		; 30�sBIOS 1.20�ȍ~
	and	al,08h
	jz	short GETVSYNC_FAILURE

	mov	ax,0ff0ah		; VSYNC���g���̎擾
	int	18h
GETVSYNC_FAILURE:
	ret
endfunc			; }


	END
