; master library - MS-DOS
;
; Description:
;	ブレーク検査フラグの読み取りと設定
;
; Function/Procedures:
;	int dos_setbreak( int breakon ) ;
;
; Parameters:
;	int breakon	0 = BREAK OFF
;			1 = BREAK ON
;
; Returns:
;	int	以前の値
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
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
;	92/11/18 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_SETBREAK
	mov	BX,SP
	; 引数
	breasksw = RETSIZE * 2

	mov	AX,3300h
	int	21h		; read Break Check
	mov	CX,DX		; CX = last sw

	mov	AX,3301h	; Set Break Check
	mov	DL,SS:[BX+breasksw]
	int	21h		;

	xor	AX,AX
	mov	AL,CL
	ret	2
endfunc

END
