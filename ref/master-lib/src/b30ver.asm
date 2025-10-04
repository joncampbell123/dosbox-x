; master library - MS-DOS - 30BIOS
;
; Description:
;	30bios�܂��� TT�̃o�[�W������Ԃ��B
;
; Procedures/Functions:
;	int bios30_getversion(void);
;
; Parameters:
;	
;
; Returns:
;	���8bit=���W���[�o�[�W����
;	����8bit=�}�C�i�[�o�[�W����
;	�� 30bios API�̒l�Ə㉺���������Ă���܂��B
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
;	�@30bios 0.05�����łُ͈�Ȓl���Ԃ邩��?
;	  30bios ���풓���Ă��Ȃ����(TT�������Ă�) ���������A0��Ԃ��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 bios30_setline/getline��b30line.asm�ɕ���
;	93/ 4/25 Initial: master.lib 0.16/b30ver.asm
;	93/ 9/13 [M0.21] 30bios_exist -> 30bios_tt_exist
;	93/12/ 9 [M0.22] TT API�Ή�
;

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETVERSION	; bios30_getversion(void) {
	_call	BIOS30_TT_EXIST
	jz	short GETVER_RET
	mov	AX,0ff00h	; 30bios API, get version
	jnc	short GETVER_30BIOS
	mov	BX,'TT'
	mov	AX,1803h	; TT API, get version
GETVER_30BIOS:
	int	18h
	xchg	AH,AL
GETVER_RET:
	ret
endfunc			; }

END
