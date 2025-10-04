; master library - MSDOS - EMS
;
; Description:
;	EMSの指定ハンドルにアロケートされているサイズを得る
;
; Function/Procedures:
;	unsigned long ems_size( unsigned handle ) ;
;
; Parameters:
;	handle  EMSハンドル
;
; Returns:
;	0     エラー
;	以外  確保されているバイト数
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
;	93/ 7/16 Initial: emssize.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_SIZE ; ems_size() {
	mov	BX,SP
	mov	DX,SS:[BX+RETSIZE*2]
	mov	AH,4ch		; function: Get Handle Page
	int	67h		; call EMM
	xor	DX,DX
	cmp	AH,0
	mov	AX,DX
	jne	short DAME
	mov	DX,BX
	shr	DX,1
	rcr	AX,1
	shr	DX,1
	rcr	AX,1
DAME:
	ret	2
endfunc		; }

END
