; master library
;
; Description:
;	VSYNC�֘A�̃O���[�o���ϐ���`
;
; Global Variables:
;	vsync_Count1
;	vsync_Count2
;	vsync_Proc
;	vsync_OldVect
;	vsync_OldMask
;	vsync_Delay
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial: grp.asm
;	93/ 8/ 8 Initial: vs.asm/master.lib 0.20
;	95/ 1/30 [M0.23] vsync_Delay�ǉ�
;
	.MODEL SMALL

	; �o�[�W����������������N����
	EXTRN _Master_Version:BYTE

	.DATA?
	public _vsync_Count1,vsync_Count1
vsync_Count1 label word
_vsync_Count1	dw	?
	public _vsync_Count2,vsync_Count2
vsync_Count2 label word
_vsync_Count2	dw	?

	public vsync_OldVect,_vsync_OldVect
_vsync_OldVect label dword
vsync_OldVect	dd	?	; int 0ah ( vsync )

	.DATA
	public _vsync_Proc,vsync_Proc
vsync_Proc label dword
_vsync_Proc	dd	0

	public _vsync_Delay,vsync_Delay
vsync_Delay label word
_vsync_Delay	dw	0

	public vsync_OldMask,_vsync_OldMask
_vsync_OldMask label byte
vsync_OldMask	db	0
	EVEN

END
