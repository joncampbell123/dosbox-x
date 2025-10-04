; master library - 30bios
;
; Description:
;	30bios�̐ݒ�\�s���̎擾
;
; Function/Procedures:
;	unsigned bios30_getlimit(void) ;
;
; Parameters:
;	none
;
; Returns:
;	��ʃo�C�g: ���
;	���ʃo�C�g: ����
;	�� 30bios API�̒l�Ə㉺���������Ă���܂��B
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�@30bios 0.20�ȍ~�A�܂��� TT 0.90�ȍ~���풓���Ă���Ɛ���Ȓl��
;	�Ԃ邩�ȁB
;	�@��L�ɂ��Ă͂܂�Ȃ��ꍇ�͏���A�����Ƃ� 25 ���Ԃ�B
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
;	93/ 9/13 Initial: b30limit.asm/master.lib 0.21
;	93/12/ 9 [M0.22] TT API�Ή�

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETLIMIT	; bios30_getlimit() {
	_call	BIOS30_TT_EXIST
	test	AL,84h		; 30bios API, TT 1.50 API
	mov	AL,0
	jz	short IGNORE
	jns	short TT_DUMMY
	mov	AX,0FF04h	; 30bios API; get limit
	int	18h
	cmp	AL,AH
	jle	short OK
	xchg	AH,AL
OK:
	ret
IGNORE:
	mov	AX,(25 shl 8) + 25
	ret
TT_DUMMY:
	mov	AX,(50 shl 8) + 25 ; TT1.5: force 25 to 50
	ret
endfunc		; }

END
