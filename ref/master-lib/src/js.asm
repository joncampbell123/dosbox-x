; master library variable
;
; Description:
;	�W���C�X�e�B�b�N�֘A�ϐ�
;
; Variables:
;	js_bexist	�W���C�X�e�B�b�N�|�[�g 0=�s�݁^1=����
;	js_stat[2]	�W���C�X�e�B�b�N�̏��( 2player�� )
;			- - - - - - - - I I T T R L D U  (1=on/0=off)
;			                R R R R I E O P
;					S S G G G F W
;					T T 2 1 H T N
;					0 1     T
;
;	js_saj_port	SAJ-98�|�[�g�A�h���X(0=�s��)
;	js_2player	�L�[�{�[�h��2�v���C���[�L���t���O(1=�L��)
;	js_shift	1=�W���C�X�e�B�b�N�����ׂ�2�v���C���[��p�Ƃ���
;
; Revision History:
;	93/ 5/ 2 Initial: js.asm/master.lib 0.16
;	94/ 2/28 [M0.23] js_2player, js_shift�ǉ�

	.MODEL SMALL

	.DATA?
	public js_stat,_js_stat
_js_stat label word
js_stat dw	?,?

	.DATA
	public js_bexist,_js_bexist
_js_bexist label word
js_bexist dw	0

	public js_saj_port,_js_saj_port
_js_saj_port label word
js_saj_port	dw	0

	public js_2player,_js_2player
_js_2player label word
js_2player	dw	0

	public js_shift,_js_shift
_js_shift label word
js_shift	dw	0

END
