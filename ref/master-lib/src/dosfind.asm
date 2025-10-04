; master library - MSDOS
;
; Description:
;	��v����t�@�C���̌���
;
; Function/Procedures:
;	int dos_findfirst( const char far * path, int attribute ) ;
;	int dos_findnext( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	1	�������BDTA�ɏ������܂ꂽ�̂ˁB
;		0	���݂��Ȃ����A�G���[�ł���
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
;	�@dos_findfirst�����s����O�ɂ́Ados_setdta��DTA��ݒ肷��̂�
;	�g�̂��߂���
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 4/ 5 Initial
;
	.MODEL SMALL
	include func.inc
	.CODE

func DOS_FINDFIRST
	mov	BX,SP
	; ����
	findpath = (RETSIZE+1)*2
	attribute = (RETSIZE+0)*2

	push	DS
	mov	CX,SS:[BX+attribute]
	lds	DX,SS:[BX+findpath]
	mov	AH,4eh
	int	21h
	sbb	AX,AX
	inc	AX
	pop	DS
	ret	6
endfunc

func DOS_FINDNEXT
	mov	AH,4fh
	int	21h
	sbb	AX,AX
	inc	AX
	ret
endfunc

END
