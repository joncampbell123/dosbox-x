; master library - MS-DOS
;
; Description:
;	DOS�����R���\�[�������o��
;
; Function/Procedures:
;	void dos_putch( int chr ) ;
;
; Parameters:
;	int chr		����
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
;	DOS�̓���f�o�C�X�R�[�� ( int 29h )�𗘗p���Ă��܂��B
;	������ DOS�ł͎g���Ȃ��Ȃ邩������܂���B
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

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTCH
	mov	BX,SP
	mov	AL,SS:[BX+ RETSIZE*2]
	int	29h
	ret	2
endfunc

END
