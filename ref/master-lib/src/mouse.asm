; master library - MOUSE
;
; Description:
;	�}�E�X�֘A�ϐ�
;
; Variables:
;	mouse_Type		�}�E�X�h���C�o�̎��(0=����/�Ȃ�,1=NEC,2=MS)
;	mouse_X			
;	mouse_Y			
;	mouse_Button		
;	mouse_ScaleX		
;	mouse_ScaleY		
;	mouse_EventRoutine	
;	mouse_EventMask		
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/24
;	93/ 7/ 6 [M0.20] mouse_type�ǉ�

	.MODEL SMALL
	.DATA

if 1
	public mouse_Type,_mouse_Type
_mouse_Type	label word
mouse_Type	dw	0	; 0 = not exist   1 = NEC   2 = MS
endif

	.DATA?
	public mouse_X,_mouse_X
_mouse_X	label word
mouse_X		dw	?

	public mouse_Y,_mouse_Y
_mouse_Y	label word
mouse_Y		dw	?

	public mouse_Button,_mouse_Button
_mouse_Button	label word
mouse_Button	dw	?

	public mouse_ScaleX,_mouse_ScaleX
_mouse_ScaleX	label word
mouse_ScaleX	dw	?
	public mouse_ScaleY,_mouse_ScaleY
_mouse_ScaleY	label word
mouse_ScaleY	dw	?

	public mouse_EventRoutine,_mouse_EventRoutine
_mouse_EventRoutine	label dword
mouse_EventRoutine	dd	?

	public mouse_EventMask,_mouse_EventMask
_mouse_EventMask	label word
mouse_EventMask	dw	?

END
