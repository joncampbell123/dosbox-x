; master library
;
; Description:
;	at98_scroll�֘A�̃O���[�o���ϐ���`
;
; Global Variables:
;	at98_VPage	�\����
;	at98_APage	�A�N�Z�X�y�[�W
;	at98_Offset	�\���A�h���X�I�t�Z�b�g(�X�N���[����ԕێ��p)
;
; Author:
;	���ˏ��F
;
; Revision History:
;	95/ 4/ 1 Initial: at98sc.asm / master.lib 0.22k
;
	.MODEL SMALL
	.DATA
	public _at98_VPage, at98_VPage
	public _at98_APage, at98_APage
	public _at98_Offset, at98_Offset
at98_APage	label word
_at98_APage	dw	0
at98_VPage	label word
_at98_VPage	dw	0
at98_Offset	label word
_at98_Offset	dw	0

END
