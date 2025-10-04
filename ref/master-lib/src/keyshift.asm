; master library - PC-9801
;
; Description:
;	�V�t�g�L�[������Ԃ̓ǂݎ��(�֐���)
;
; Function/Procedures:
;	int key_shift(void) ;
;
; Parameters:
;	None
;
; Returns:
;	int 	1	SHIFT
;		2	CAPS
;		4	��
;		8	GRPH
;		16	CTRL
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
;	�}�N���ł��ƃ��[�N�G���A���ړǂݎ��̂��߁A�L�[�{�[�h�̓ǂݎ���
;	������ TSR �Ƃ̑������悭����܂���B
;	���S�̂��߁A�֐��ł��g���ׂ��ł��B
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
;	93/ 5/ 1 Initial:keyshift.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE

func KEY_SHIFT
	mov	AH,2
	int	18h
	mov	AH,0
	ret
endfunc

END
