; master library - MS-DOS
;
; Description:
;	�t�@�C������P���[�h�ǂݍ���
;	�t�@�C������P���[�h��������
;
; Function/Procedures:
;	int file_getw( void ) ;
;	void file_putw( int i ) ;
;
; Parameters:
;	i	�������ރf�[�^( ���ʃo�C�g�A��ʃo�C�g�̏��ɏ������ )
;
; Returns:
;	int	(file_getw)�ǂݍ��񂾃f�[�^
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
;	file_getw: �r����EOF�ɑ��������ꍇ�A�ǂݍ��ޒl�͂�����܂��B
;		   ���ł� EOF�Ȃ�� -1 ��Ԃ��܂����A�ǂ���̏ꍇ�ł��A
;		   file_Eof�ϐ��ł̂�EOF�m�F���s���Ă��������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/25 Initial
;	93/ 1/13 ��������܂������ĂȂ���������(^^;

	.MODEL SMALL
	include func.inc
	EXTRN FILE_GETC:CALLMODEL
	EXTRN FILE_PUTC:CALLMODEL

	.CODE
func FILE_GETW
	call	FILE_GETC	; lo
	push	AX
	call	FILE_GETC	; hi
	mov	BX,AX
	pop	AX
	mov	AH,BL
	ret
	EVEN
endfunc

func FILE_PUTW
	mov	BX,SP
	; ����
	i	= RETSIZE * 2
	mov	AX,SS:[BX+i]
	push	AX
	push	AX
	call	FILE_PUTC	; lo
	pop	AX
	mov	AL,AH
	push	AX
	call	FILE_PUTC	; hi
	ret	2
endfunc

END
