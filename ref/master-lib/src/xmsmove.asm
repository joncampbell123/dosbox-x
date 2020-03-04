; Master Library - XMS
;
; Description:
;	XMS・メインメモリ間および同士のメモリ複写
;
; Functions:
;	int xms_movememory( long destOff, unsigned destHandle, long srcOff, unsigned srcHandle, long Length )
;
; Parameters:
;	long destOff		転送先のアドレス
;	unsigned destHandle	転送先のハンドル(0=メインメモリ)
;	long srcOff		転送元のアドレス
;	unsigned srcHandle	転送元のハンドル(0=メインメモリ)
;	long Length		転送するバイト数
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
;	94/ 9/15 [M0.23] LARGECODE BUGFIX, 飛んでた

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_MOVEMEMORY
	mov	BX,SP
	push	SI
	push	DS

	mov	AX,DS
	mov	ES,AX
	mov	AX,SS
	mov	DS,AX
	lea	SI,[BX+(RETSIZE)*2]	; length
	mov	AH,0bh
	call	ES:[xms_ControlFunc]
	pop	DS
	pop	SI
	ret 16
endfunc

END
