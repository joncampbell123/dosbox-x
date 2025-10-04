; master library - MS-DOS
;
; Description:
;	�T�u�f�B���N�g���̍폜
;
; Function/Procedures:
;	int dos_rmdir( const char * path ) ;
;	function dos_rmdir( path:string ) : boolean ;
;
; Parameters:
;	char * path	�폜����f�B���N�g���p�X
;
; Returns:
;	1 = ����
;	0 = ���s
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/14 Initial
;	93/ 5/15 [M0.16] dos_axdx�g�p

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN DOS_AXDX:CALLMODEL

func DOS_RMDIR
	mov	BX,SP
	; ����
	path	= RETSIZE*2

	mov	AH,3ah
	push	AX
	_push	SS:[BX+path+2]
	push	SS:[BX+path]
	_call	DOS_AXDX
	sbb	AX,AX
	inc	AX
	ret	(DATASIZE*2)
endfunc

END
