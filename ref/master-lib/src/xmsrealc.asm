; Master Library - XMS
;
; Description:
;	XMSメモリを再確保する
;
; Functions:
;	unsigned xms_reallocate( unsigned handle, long memsize )
;
; Parameters:
;	handle   すでに確保された XMSハンドル
;	memsize  新しいバイト数
;
; Returns:
;	0 = 失敗
;	1 = 成功
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
;	93/ 7/16 Initial: xmsrealc.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_REALLOCATE	; xms_reallocate() {
	mov	BX,SP
	; 引数
	handle  = (RETSIZE+2)*2
	memsize = (RETSIZE+0)*2

	mov	AX,SS:[BX+memsize]
	mov	DX,SS:[BX+memsize+2]
	mov	CX,1024
	div	CX
	add	DX,-1
	adc	AX,0
	mov	DX,SS:[BX+handle]
	mov	BX,AX

	mov	AH,0fh
	call	[xms_ControlFunc]
	ret 6
endfunc			; }

END
