; master library module
; Description:
;	VTEXT�֘A�ϐ�
;
; Variables:
;	TextVramSize:
;		�e�L�X�g��ʑS�̂̕�����, ������AT�݊��@��p
;
;	VTextState:
;		��ԕϐ�
;		bit  0: 0=�������ݑ��`��      1=�����I�ɕ`��͍s��Ȃ�
;		bit 14: 0=����                1=�e�L�X�g�O���t�B�b�N���[�h��
;		bit 15: 0=�`�拖��            1=�`��֎~(�O���t�B�b�N���[�h)
;
;		���e�L�X�g�O���t�B�b�N���[�h:
;		   DOS/V�̃e�L�X�g�\����master.lib��vtextx_start�ɂ����
;		 �O���t�B�b�N���[�h�ŃG�~�����[�V�������Ă�����

; Revision History:
;	94/ 4/ 9 Initial: vt.asm/master.lib 0.23

	.MODEL SMALL
	.DATA
	public _TextVramSize,TextVramSize
	public _VTextState,VTextState

TextVramSize label word
_TextVramSize dw 4000

VTextState label word
_VTextState dw 0

END
