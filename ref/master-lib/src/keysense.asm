; Master Library - Key Sense
;
; Description:
;	���݂̃L�[�R�[�h�O���[�v���̃L�[�̉�����Ԃ𒲂ׂ�B
;
; Function/Procedures:
;	int key_sense( int keygroup ) ;
;
; Parameters:
;	char keygroup	�L�[�R�[�h�O���[�v
;
; Returns:
;	�L�[�̉������
;
; Binding Target:
;	TASM
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: 8086
;
; Compiler/Assembler:
;	TASM 2.51
;
; Note:
;	�L�[�o�b�t�@�Ƃ͊֌W�Ȃ����삷��B
;	�L�[���s�[�g�ɂ���ĂƂ肱�ڂ����ꂪ����̂œ�x���s��
;	���ʂ�or���Ƃ邱��
;	��[���ƁA��ʐl�d�l(tab 8��MASM Mode)�Ȃ̂ň��S���Ă��������B
;
; Author:
;	SuCa
;
; Rivision History:
;	93/06/16 initial
;	93/ 6/17 Initial: keysense.asm/master.lib 0.19

	.MODEL SMALL
	include func.inc

	.CODE

func	KEY_SENSE
	keygroup = RETSIZE*2

	mov	bx,sp
	mov	ax,ss:[bx+keygroup]
	mov	ah,04h
	int	18h
	mov	al,ah
	mov	ah,0
	ret	2
endfunc

END
