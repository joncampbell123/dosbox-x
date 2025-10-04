; master library - MS-DOS
;
; Description:
;	�t�@�C���̃^�C���X�^���v��ύX����
;
; Function/Procedures:
;	int file_lsettime( unsigned long _filetime ) ;
;	int file_settime( unsigned _date,  unsigned _time ) ;
;
; Parameters:
;	unsigned _filetime ����(���=_date,����=_time)
;			   ��file_time()�̖߂�l�Ɠ���
;	unsigned _date	   ���t(MS-DOS�t�@�C���Ǘ��l��)
;	unsigned _time	   ����(�V)
;
; Returns:
;	1    ����
;	0    ���s (�J����Ă��Ȃ�)
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
;	���ۂɂ́A�N���[�Y�����Ƃ��Ɏ������ݒ肳��܂��B
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
;	93/ 1/16 SS!=DS�Ή�

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Handle:WORD

	.CODE

func FILE_LSETTIME
	; ���x���t���邾���Œ��g�͊��S�ɓ���
endfunc
func FILE_SETTIME
	mov	BX,SP
	; ����
	_date = (RETSIZE+1)*2
	_time = (RETSIZE+0)*2
	mov	DX,SS:[BX+_date]
	mov	CX,SS:[BX+_time]
	mov	BX,file_Handle
	mov	AX,5701h
	int	21h
	sbb	AX,AX
	inc	AX
	ret	4
endfunc

END
