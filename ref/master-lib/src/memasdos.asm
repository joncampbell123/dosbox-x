; master library - MS-DOS
;
; Description:
;	�w��T�C�Y��DOS�̃��������m�ۂ��A�������}�l�[�W���֊��蓖�Ă�
;
; Functions/Procedures:
;	int mem_assign_dos( unsigned parasize ) ;
;
; Noters:
;	���W�X�^�� AX�̂ݔj�󂵂܂��B
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 3/23 Initial (0.14)
;	93/ 3/23 bugfix (0.15)

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	mem_MyOwn:WORD

	.CODE
	EXTRN	MEM_ASSIGN:CALLMODEL

	; int mem_assign_dos( unsigned parasize ) ;
	; �j��: AX�̂�
	; �������s���Ȃ� cy=1
func MEM_ASSIGN_DOS
	push	BX
	mov	BX,SP
	; ����
	parasize = (RETSIZE+1)*2

	mov	BX,SS:[BX+parasize]
	mov	AH,48h		; �m�ۂ���
	int	21h
	jc	short NOMEM
	push	AX
	push	BX
	call	MEM_ASSIGN
	xor	AX,AX
	mov	mem_MyOwn,1
NOMEM:
	neg	AX
	pop	BX
	ret	2
endfunc

END
