; master library module
;
; Description:
;	�t�@�C���|�C���^�̈ړ�
;
; Functions/Procedures:
;	long dos_seek( int fh, long offs, int mode ) ;
;
; Parameters:
;	
;	mode	0=�擪����  1=���݈ʒu����  2=�t�@�C����������
;
; Returns:
;	long	InvalidData  (cy=1)	���s,mode��0,1,2�̂ǂ�ł��Ȃ�
;		InvalidHandle(cy=1)	���s,�t�@�C���n���h��������
;		else	     (cy=0)	����,�V�����擪����̃I�t�Z�b�g
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	93/ 3/17 Initial: master.lib
;	93/10/15 [M0.21] bugfix(^^;
;

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_SEEK
	push	BP
	mov	BP,SP
	fh	= (RETSIZE+4)*2
	offs	= (RETSIZE+2)*2
	mode	= (RETSIZE+1)*2

	mov	BX,[BP+fh]
	mov	DX,[BP+offs]
	mov	CX,[BP+offs+2]
	mov	AL,[BP+mode]
	mov	AH,42h
	int	21h
	jnc	short SUCCESS
	neg	AX	; cy=1
	cwd
SUCCESS:
	pop	BP
	ret	8
endfunc

END
