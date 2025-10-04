; master library - MS-DOS
;
; Description:
;	�J�����g�f�B���N�g���̕ύX
;
; Function/Procedures:
;	int dos_chdir( const char * path ) ;
;	function dos_chdir( path:string ) : boolean ;
;
; Parameters:
;	char * path	�f�B���N�g��
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
;	92/11/17 Initial
;	93/ 5/15 [M0.16] bugfix & dos_axdx�g�p

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN DOS_AXDX:CALLMODEL

func DOS_CHDIR
	mov	BX,SP
	; ����
	path	= RETSIZE*2

	mov	AH,3bh
	push	AX
	_push	SS:[BX+path+2]
	push	SS:[BX+path]
	call	DOS_AXDX
	sbb	AX,AX
	inc	AX
	ret	(DATASIZE*2)
endfunc

END
