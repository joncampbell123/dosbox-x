; Master Library - XMS
;
; Description:
;	XMSドライバの存在を確認し、コントロール関数を得る
;
; Functions:
;	int xms_exist(void) ;
;
; Parameters:
;	none
;
; Returns:
;	1 = 存在。xms_ControlFuncにコントロール関数のアドレスを得た
;	0 = 不在。
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 80286
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
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_EXIST
	mov	AX,4300h	; is there?
	int	2fh
	cmp	AL,80h
	mov	AX,0
	jne	short	NOTHERE

	mov	AX,4310h	; get control function
	int	2fh
	mov	WORD PTR [xms_ControlFunc],BX
	mov	WORD PTR [xms_ControlFunc+2],ES
	mov	AX,1
NOTHERE:
	ret
endfunc

END
