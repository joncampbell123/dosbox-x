; master library - MS-DOS
;
; Description:
;	MS-DOS�t�@���N�V�����R�[��(AX=val, DS,DX=������)
;	(Pascal������)
;
; Function/Procedures:
;	long dos_axdx( int axval, void * dxstr ) ;
;	function dos_axdx( axval:integer; dxstr:string ) : LongInt ;
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
;	93/ 5/15 Initial: dosp.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN STR_PASTOC:CALLMODEL

func	DOS_AXDX	; {
	push	BP
	mov	BP,SP
	sub	SP,256
	; ����
	axval	= (RETSIZE+1+DATASIZE)*2
	dxstr	= (RETSIZE+1)*2
	; �ϐ�
	cstr	= -256

	push	BX
	push	CX
	_push	SS
	lea	AX,[BP+cstr]
	push	AX
	_push	[BP+dxstr+2]
	push	[BP+dxstr]
	_call	STR_PASTOC
	pop	CX
	pop	BX

	push	DS
	push	SS
	pop	DS
	lea	DX,[BP+cstr]
	mov	AX,[BP+axval]
	int	21h
	pop	DS

	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX	; �G���[�Ȃ畄�����],cy=1
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
