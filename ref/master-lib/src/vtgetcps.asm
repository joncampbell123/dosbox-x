; master library - vtext
;
; Description:
;	�e�L�X�g��ʂ̃J�[�\���ʒu�擾
;
; Function/Procedures:
;	long vtext_getcurpos( void )
;
; Parameters:
;	none
;
; Returns:
;	long	HIWORD(DX) = y���W( 0 = ��[ )
;		LOWORD(AX) = x���W( 0 = ���[ )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PC/AT �̃��[�N�G���A(page 0)�𒼐ڎQ�Ƃ��Ă��܂��B
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/07/28 Initial
;	93/08/10 �֐������� _ �����
;	94/01/16 �֐����̕ύX(�� get_cursor_pos_at)
;	94/ 4/ 9 Initial: vtgetcps.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE

func VTEXT_GETCURPOS	; vtext_getcurpos() {
	xor	AX,AX
	mov	ES,AX
	mov	DX,AX
	mov	AX,ES:[0450h]
	xchg	DL,AH
	ret
endfunc			; }

END
