; Master Library - XMS
;
; Description:
;	XMSメモリを開放する
;
; Functions:
;	void xms_free( unsigned handle )
;
; Parameters:
;	unsigned handle	XMSメモリハンドル
;
; Returns:
;	none
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 80286
;
; Notes:
;	xms_existで存在と返されていない状態で実行すると暴走します。
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

func XMS_FREE
	mov	BX,SP
	; 引数
	handle = (RETSIZE)*2

	mov	DX,SS:[BX+handle]
	mov	AH,0ah
	call	[xms_ControlFunc]
	ret 2
endfunc

END
