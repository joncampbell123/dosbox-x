; Master Library - XMS
;
; Description:
;	XMSのコントロール関数のアドレス
;
; Variables:
;	void far * xms_ControlFunc ;
;
; Notes:
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/23 Initial
;	93/ 7/16 [M0.20] _xms_ControlFunc -> xms_ControlFuncに改名

	.MODEL SMALL
	.DATA

	public xms_ControlFunc
xms_ControlFunc dd 0

END
