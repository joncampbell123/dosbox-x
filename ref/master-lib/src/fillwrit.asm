; master library - MS-DOS
;
; Description:
;	�t�@�C���ɏ�������(long version)
;
; Function/Procedures:
;	int file_lwrite( const char far * buf, unsigned long bsize ) ;
;
; Parameters:
;	char far * buf		�������ރf�[�^�̐擪�A�h���X
;	unsigned long bsize	�������ރo�C�g��(0�͋֎~)
;
; Returns:
;	1 = ����
;	0 = ���s
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
;	92/12/11 Initial
;	93/ 2/ 2 bugfix

	.MODEL SMALL
	include func.inc
	EXTRN FILE_WRITE:CALLMODEL

	.CODE

func FILE_LWRITE
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	buf	= (RETSIZE+3)*2
	bsize	= (RETSIZE+1)*2

	mov	SI,[BP+buf]	;
	mov	DI,[BP+buf+2]	; DI:SI
	mov	CL,4
	mov	AX,SI
	shr	AX,CL
	add	DI,AX
	and	SI,0fh		; DI:SI�͐��K�����ꂽ huge pointer�ɂȂ���

	cmp	word ptr [BP+bsize+2],0
	je	short READLAST
READLOOP:
	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_WRITE
	test	AX,AX
	jz	short FINISH
	add	DI,800h

	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_WRITE
	test	AX,AX
	jz	short FINISH
	add	DI,800h

	dec	word ptr [BP+bsize+2]
	jne	short READLOOP
READLAST:

	push	DI
	push	SI
	push	word ptr [BP+bsize]
	call	FILE_WRITE
FINISH:

	pop	DI
	pop	SI
	pop	BP
	ret	8
endfunc

END
