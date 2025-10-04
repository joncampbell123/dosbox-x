; master library - MSDOS
;
; Description:
;	���C���������̊m��
;
; Function/Procedures:
;	unsigned dos_allocate( unsigned para ) ;
;
; Parameters:
;	unsigned para	�m�ۂ���p���O���t�T�C�Y
;
; Returns:
;	unsigned �m�ۂ��ꂽDOS�������u���b�N�̃Z�O�����g
;		 0 �Ȃ烁����������Ȃ��̂Ŏ��s�ł��
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
;	93/ 1/31 Initial dosalloc.asm

	.MODEL SMALL
	include func.inc
	.CODE

func DOS_ALLOCATE
	mov	BX,SP
	mov	BX,SS:[BX+RETSIZE*2]	; para
	mov	AH,48h		; �m�ۂ���
	int	21h
	cmc
	sbb	CX,CX
	and	AX,CX		; CX=seg, ���s�Ȃ� 0
	ret	2
endfunc

END
