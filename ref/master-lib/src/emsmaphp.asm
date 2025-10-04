; master library - EMS - MAPHANDLEPAGE
;
; Description:
;	ハンドルページのマップ
;
; Function/Procedures:
;	int ems_maphandlepage( int phys_page, unsigned handle, unsigned log_page )
;
; Parameters:
;	phys_page	物理ページフレーム番号(0～)
;	handle		オープンされたEMSハンドル
;	log_page	EMSハンドル内の相対ページ番号(0～)
;
; Returns:
;	0		成功
;	80h～8bh	エラー(EMSエラーコード)
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
;	93/ 7/19 Initial: emsmaphp.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_MAPHANDLEPAGE	; ems_maphandlepage() {
	push	BP
	mov	BP,SP
	; 引数
	phys_page  = (RETSIZE+3)*2
	handle     = (RETSIZE+2)*2
	log_page   = (RETSIZE+1)*2

	mov	AH,44h
	mov	AL,[BP+phys_page]
	mov	BX,[BP+log_page]
	mov	DX,[BP+handle]

	int	67h
	mov	AL,AH
	mov	AH,0

	pop	BP
	ret	6
endfunc		; }

END
