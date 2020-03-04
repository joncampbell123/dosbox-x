; master library - MSDOS - EMS
;
; Description:
;	EMSメモリの確保
;
; Function/Procedures:
;	int ems_reallocate( unsigned handle, unsigned long size ) ;
;
; Parameters:
;	handle  既に確保されている EMS ハンドル
;	size	新しいバイト数
;
; Returns:
;	int	0     成功
;		else  EMSエラーコード
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
;	93/ 7/17 Initial: emsrealc.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_REALLOCATE	; ems_reallocate() {
	mov	BX,SP
	; 引数
	handle	= (RETSIZE+2)*2
	size_hi	= (RETSIZE+1)*2
	size_lo	= (RETSIZE+0)*2

	mov	DX,SS:[BX+handle]
	mov	CX,SS:[BX+size_lo]
	mov	BX,SS:[BX+size_hi]
	shl	CX,1
	rcl	BX,1
	shl	CX,1
	rcl	BX,1
	cmp	CX,1
	sbb	BX,-1
	mov	AH,51h		; function: Reallocate Pages
	int	67h		; call EMM
	mov	AL,AH
	xor	AH,AH
	ret	6
endfunc		; }

END
