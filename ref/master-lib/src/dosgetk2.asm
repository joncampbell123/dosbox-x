; master library - MS-DOS
;
; Description:
;	DOS�E�W�����͂���̕����ǂݎ��R
;	(^C�Ŏ~�܂�Ȃ�, ���͂��Ȃ��Ă��҂��Ȃ�,���͂Ȃ���CTRL+@����ʂł���)
;
; Function/Procedures:
;	int dos_getkey2( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 .. 255 ���͂��ꂽ����
;		-1 ...... ���͂Ȃ�
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
;	92/12/18 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETKEY2
	mov	AH,6
	mov	DL,0FFh
	int	21h
	mov	AH,0
	jnz	short OK
	dec	AX
OK:
	ret
endfunc

END
