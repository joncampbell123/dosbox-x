; master library - MSDOS
;
; Description:
;	���C���������u���b�N�̊J��
;
; Function/Procedures:
;	void mem_free( unsigned seg ) ;
;	void dos_free( unsigned seg ) ;		�����̓�͓������̂ł�
;
; Parameters:
;	unsigned seg	DOS�������u���b�N�̃Z�O�����g
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 2.0 or later
;
; Notes:
;	AX�ȊO�̑S���W�X�^��ۑ����܂�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 1/31 Initial dosfree.asm(from memalloc.asm)
;	93/ 3/29 ������bugfix

	.MODEL SMALL
	include func.inc
	.CODE

	public DOS_FREE
func MEM_FREE
DOS_FREE label CALLMODEL
	push	BP
	push	ES
	mov	BP,SP
	mov	ES,[BP+(RETSIZE+2)*2]	; seg
	mov	AH,49h
	int	21h
	pop	ES
	pop	BP
	ret	2
endfunc

END
