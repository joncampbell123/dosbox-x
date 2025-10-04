; master library - EMS - SAVEPAGEMAP
;
; Description:
;	ページマップのセーブ
;
; Function/Procedures:
;	int ems_savepagemap( unsigned handle )
;
; Parameters:
;	handle		オープンされたEMSハンドル
;
; Returns:
;	0		成功
;	80h～8dh	エラー(EMSエラーコード)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 3.2
;
; Notes:
;	
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
;	93/ 7/19 Initial: emssavem.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_SAVEPAGEMAP	; ems_savepagemap() {
	mov	BX,SP
	; 引数
	handle     = (RETSIZE+0)*2

	mov	AH,47h
	mov	DX,SS:[BX+handle]

	int	67h
	mov	AL,AH
	mov	AH,0

	ret	2
endfunc		; }

END
