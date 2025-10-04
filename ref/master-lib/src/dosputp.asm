; master library - MS-DOS
;
; Description:
;	�W���o�͂Ƀp�X�J����������o�͂���
;
; Function/Procedures:
;	void dos_putp( const char * passtr ) ;
;
; Parameters:
;	char * passtr	�p�X�J��������
;
; Returns:
;	none
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
;	92/12/19 Initial
;	93/ 1/16 SS!=DS�Ή�

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTP
	mov	BX,SP
	push	DI
	_push	DS

	; ����
	passtr	= RETSIZE * 2

	_lds	BX,SS:[BX+passtr]
	lea	DX,[BX+1]
	mov	CL,[BX]
	mov	CH,0
	mov	AH,40h		; 40h: DOS WRITE
	mov	BX,1		; BX = 1(STDOUT)
	int	21h

	_pop	DS
	pop	DI
	ret	DATASIZE*2
	EVEN
endfunc

END
