; master library - timer
;
; Description:
;	�^�C�}���荞�݊֘A�̃O���[�o���ϐ���`
;
; Global Variables:
;	timer_Count
;	timer_Proc
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 3/20 Initial: ti.asm/master.lib 0.23
;
	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA
	public _timer_Count,timer_Count
timer_Count label dword
_timer_Count	dd	0

	public _timer_Proc,timer_Proc
timer_Proc label dword
_timer_Proc	dd	0

END
