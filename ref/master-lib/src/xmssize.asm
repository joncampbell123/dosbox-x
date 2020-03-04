; master library - MS-DOS - XMS
;
; Description:
;	XMSハンドルの確保されたバイト数を得る
;
; Function/Procedures:
;	unsigned long xms_size( unsigned handle )
;
; Parameters:
;	handle   すでに確保されている XMS ハンドル
;
; Returns:
;	0    エラー(ハンドルが無効、ドライバがこのファンクションを知らない)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
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
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 7/16 Initial: xmssize.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_SIZE	; xms_size() {
	mov	BX,SP
	; 引数
	handle = (RETSIZE)*2

	mov	DX,SS:[BX+handle]
	mov	AH,0eh
	call	[xms_ControlFunc]
	neg	AX
	and	AX,1024
	mul	DX
	ret 2
endfunc		; }

END
