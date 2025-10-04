; master library - MS-DOS
;
; Description:
;	MS-DOS�t�@���N�V�����R�[��(AX=val, DS,DX=������)
;
; Function/Procedures:
;	long dos_axdx( int axval, void * string ) ;
;
; Parameters:
;	axval	AX�Ɋi�[����l
;	string	DS,DX�Ɋi�[���镶����ւ̃|�C���^
;
; Returns:
;	���16bit(DX) -1 = ���s(cy=1), ����16bit(AX) = DOS �G���[�R�[�h
;	               0 = ����(cy=0),               = AX�̒l
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
; Assembly Language Note:
;	���ɂ� BX,CX,SI,DI��n���܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 5/15 Initial: dosc.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE

func	DOS_AXDX	; {
	push	BP
	mov	BP,SP
	; ����
	axval	= (RETSIZE+1+DATASIZE)*2
	string	= (RETSIZE+1)*2

	_push	DS
	_lds	DX,[BP+string]
	mov	AX,[BP+axval]
	int	21h
	_pop	DS
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX	; �G���[�Ȃ畄�����],cy=1
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
