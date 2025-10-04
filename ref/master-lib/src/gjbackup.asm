; master library - PC98
;
; Description:
;	�O���f�[�^�����C���������ɑޔ��C��������
;
; Function/Procedures:
;	int gaiji_backup( void ) ;		(�ޔ�)
;	int gaiji_restore( void ) ;		(����)
;
;
; Parameters:
;	none
;
; Returns:
;	int	1 = ����
;		0 = ���s( �ޔ𥕜���񐔕s��v, ���C��������������Ȃ��Ȃ� )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���ɑޔ����Ă���̂ɂ�����x�ޔ����悤�Ƃ���ƃG���[�ɂȂ�܂��B
;	�����l�ɁA�J�����A���Q����s�͂ł��܂���B
;	�ޔ��A�J���ƌ��݂Ȃ�Ή���ł����s�ł��܂��B
;
;	�e�L�X�g��ʂ��\������Ă���Ɖ�ʂ̊�����������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/ 4 Initial
;	93/ 3/23 �������}�l�[�W���g�p

	.MODEL SMALL
	include func.inc
	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

GAIJISIZE equ 256*2*16

	.DATA
backup_mseg	dw	0	; Main Memory Segment

	.CODE

	EXTRN	GAIJI_READ_ALL:CALLMODEL
	EXTRN	GAIJI_WRITE_ALL:CALLMODEL

func GAIJI_BACKUP	; {
	xor	AX,AX
	cmp	backup_mseg,AX
	jne	short B_END		; ���łɑޔ����Ă�Ȃ�G���[

	mov	AX,GAIJISIZE/16
	push	AX
	call	HMEM_ALLOC
	or	AX,AX
	jz	short B_END

	mov	backup_mseg,AX
	push	AX
	xor	AX,AX
	push	AX
	call	GAIJI_READ_ALL
	mov	AX,1
B_END:
	ret
endfunc			; }

func GAIJI_RESTORE	; {
	mov	AX,backup_mseg
	test	AX,AX
	jz	short R_FAULT

	push	AX		; ����push�̂݁AHMEM_FREE�̈��������
	push	AX
	xor	AX,AX
	mov	backup_mseg,AX
	push	AX
	call	GAIJI_WRITE_ALL
	call	HMEM_FREE
	mov	AX,1
R_FAULT:
	ret
endfunc		; }

END
