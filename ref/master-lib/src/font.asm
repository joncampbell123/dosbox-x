; master library
;
; Global Variables:
;	unsigned font_AnkSeg	256�p�^�[�����̗̈�ւ̃Z�O�����g
;	unsigned font_AnkSize	super_patsize[]�Ɠ��l�̓��e�ŁA�p�^�[���T�C�Y
;	unsigned font_AnkPara	1�����ɕK�v�ȃp���O���t��
;
; Revision History:
;	93/ 1/27 Initial
;	93/ 2/24 super.lib����
;	94/ 1/24 font_AnkSize, font_AnkPara�ǉ�

	.MODEL SMALL
	.DATA?
	public font_AnkPara
	public _font_AnkPara
_font_AnkPara label word
font_AnkPara dw ?

	.DATA
	public font_AnkSeg
	public _font_AnkSeg
_font_AnkSeg label word
font_AnkSeg dw 0

	public font_AnkSize
	public _font_AnkSize
_font_AnkSize label word
font_AnkSize dw 0

END
