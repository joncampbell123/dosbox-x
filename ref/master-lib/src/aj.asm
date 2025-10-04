FASTSTICK equ 1
; master library variable
;
; Description:
;	AT�݊��@�W���C�X�e�B�b�N�֘A�ϐ�
;
; Variables:
;	at_js_count		�W���C�X�e�B�b�N�̈�莞�ԕ��̃��[�v��(�v��)
;	at_js_x1,at_js_y1	�W���C�X�e�B�b�N1��x,y�J�E���g�l(�v��)
;	at_js_x2,at_js_y2	�W���C�X�e�B�b�N2��x,y�J�E���g�l(�v��)
;	at_js_mintime		�W���C�X�e�B�b�N�̒l�̉���(�ݒ�)
;	at_js_maxtime		�W���C�X�e�B�b�N�̒l�̏��(�ݒ�)
;	at_js_min		�W���C�X�e�B�b�N�̒l�̉����J�E���g(�Z�o)
;	at_js_max		�W���C�X�e�B�b�N�̒l�̏���J�E���g(�Z�o)
;
; Revision History:
;	94/ 5/17 Initial: aj.asm/master.lib 0.23

	.MODEL SMALL

	.DATA?

	.DATA
	public	at_js_mintime, _at_js_mintime
	public	at_js_maxtime, _at_js_maxtime

_at_js_mintime label word
at_js_mintime	dw	30

_at_js_maxtime label word
at_js_maxtime	dw	1700

if FASTSTICK
	public at_js_fast, _at_js_fast
_at_js_fast label word
at_js_fast	dw	0
endif

	public	AT_JS_RESID,       _AT_JS_RESID
_AT_JS_RESID label byte
AT_JS_RESID	db "MASTER_AT_JS"

	public	at_js_resseg,      _at_js_resseg
_at_js_resseg label word
at_js_resseg	dw	0


	EVEN

	.DATA?

	public at_js_count, _at_js_count
_at_js_count label word
at_js_count	dw	?

	public at_js_x1, _at_js_x1
_at_js_x1 label word
at_js_x1	dw	?

	public at_js_y1, _at_js_y1
_at_js_y1 label word
at_js_y1	dw	?

	public at_js_x2, _at_js_x2
_at_js_x2 label word
at_js_x2	dw	?

	public at_js_y2, _at_js_y2
_at_js_y2 label word
at_js_y2	dw	?

	public at_js_min, _at_js_min
_at_js_min label word
at_js_min	dw	?

	public at_js_max, _at_js_max
_at_js_max label word
at_js_max	dw	?

END
