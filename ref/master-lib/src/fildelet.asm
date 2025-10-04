; master library - MS-DOS
;
; Description:
;	�t�@�C���̍폜
;
; Function/Procedures:
;	int file_delete( const char * filename ) ;
;	function file_delete( filename:string ) : boolean ;
;
; Parameters:
;	char * filename	�t�@�C����
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
;	93/ 5/15 [M0.16] dos_axdx�g�p
;	93/ 7/ 9 [M0.20] �Ё[���łƂ�(��)

	.MODEL SMALL
	include func.inc
	EXTRN DOS_AXDX:CALLMODEL

	.CODE
func FILE_DELETE
	mov	BX,SP
	; ����
	filename = RETSIZE*2

	mov	AH,41h			; �t�@�C���̍폜
	push	AX
	_push	SS:[BX+filename+2]
	push	SS:[BX+filename]
	call	DOS_AXDX
	sbb	AX,AX
	inc	AX

	ret	DATASIZE*2
	EVEN
endfunc

END
