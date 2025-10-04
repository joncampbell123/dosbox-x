; master library - DOS - file - size
;
; Description:
;	�t�@�C���̑傫���𓾂�
;
; Function/Procedures:
;	long dos_filesize( int fh ) ;
;
; Parameters:
;	fh  �t�@�C���n���h��
;
; Returns:
;	InvalidHandle   (cy=1) �n���h��������
;	0�`             (cy=0) �t�@�C���̑傫��
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
;	94/ 2/14 Initial: dosfsize.asm/master.lib 0.22a
;	95/ 1/21 [M0.23] BUGFIX

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_FILESIZE	; dos_filesize() {
	mov	BX,SP
	filehandle = (RETSIZE+0)*2
	mov	BX,SS:[BX+filehandle]

	mov	AX,4201h	; ���݈ʒu�𓾂�
	xor	CX,CX
	mov	DX,CX
	int	21h
	jc	short FAULT

	push	SI
	push	DI

	push	AX		; ���݈ʒu��push
	push	DX
	xor	DX,DX
	mov	AX,4202h	; ������ (CX,DX�͊��� 0)
	int	21h
	mov	SI,AX		; �����ʒu�� DI,SI�ɕۑ�
	mov	DI,DX

	pop	CX
	pop	DX
	mov	AX,4200h	; ���̈ʒu�֖߂�
	int	21h

	mov	AX,SI
	mov	DX,DI
	pop	DI
	pop	SI
	ret	2
FAULT:
	neg	AX		; InvalidHandle
	sbb	DX,DX		; �K��-1, cy=1
	ret	2
endfunc		; }

END
