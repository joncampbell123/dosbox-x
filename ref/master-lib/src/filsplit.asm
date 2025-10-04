; master library - 
;
; Description:
;	path ��4�̍\���v�f�ɕ�������
;
; Functions/Procedures:
;	void file_splitpath(const char *path, char *drv, char *dir, char *base, char *ext)
;
; Parameters:
;	path	�������������p�X��( e.g. "A:\HOGE\ABC.C" )	NULL�֎~
;	drv	�h���C�u�����̊i�[��( "A:" )			NULL��
;	dir	�p�X�����̊i�[��( "\HOGE\" )			NULL��
;	base	�t�@�C���������̊i�[��("ABC")			NULL��
;	ext	�g���q�����̊i�[��(".C")			NULL��
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
;	�Edrv�� 3byte�p�ӂ��Ă���Έ��S�ł�
;	�Edir�� path���ۂ��ƃR�s�[�ł��邾���̗̈悪�K�v�ł�
;	�Ebase�́A9byte�p�ӂ��Ă���Έ��S�ł�
;	�Eext�� 6byte�p�ӂ��Ă���Έ��S�ł�
;
;	�Edrv,dir,base,ext��NULL�ɂ���Ώ������݂܂���B
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
;	94/ 5/30 Initial: filsplit.asm/master.lib 0.23
;	95/ 3/10 [M0.22k] BUGFIX: LARGE drv,dir��NULL�̂Ƃ������Ă�
;	95/ 4/12 [M0.22k] BUGFIX: LARGE ext���������i�[����Ȃ�����

	.MODEL SMALL
	include func.inc
	.code

func FILE_SPLITPATH		; file_splitpath() {
	push	BP
	mov	BP,SP
	sub	SP,2
	push	SI
	push	DI

	;
	path = (RETSIZE+1+4*DATASIZE)*2
	drv  = (RETSIZE+1+3*DATASIZE)*2
	dir  = (RETSIZE+1+2*DATASIZE)*2
	base = (RETSIZE+1+1*DATASIZE)*2
	ext  = (RETSIZE+1+0*DATASIZE)*2

	;
	idir = -2

	xor	AX,AX
	mov	[BP+idir],AX
	xor	DI,DI		; ibase
	xor	DX,DX		; iext
	xor	CX,CX		; kflag
	xor	BX,BX		; i
	_push	DS
	_lds	SI,[BP+path]

NEXT:
	mov	AL,[BX+SI]
	cmp	AL,0
	je	short BREAK

SLOOP:
	inc	BX

	dec	CX
	jz	short NEXT

	cmp	AL,81h
	jl	short ISASCII
	cmp	AL,9fh
	jle	short ISKANJI
	cmp	AL,0e0h
	jl	short ISASCII
	cmp	AL,0fch
	jle	short ISKANJI
ISASCII:
	cmp	AL,':'
	je	short COLON
	cmp	AL,'\'
	je	short SLASH
	cmp	AL,'/'
	je	short SLASH
	cmp	AL,'.'
	je	short DOT

	mov	AL,[BX+SI]
	cmp	AL,0
	jne	short SLOOP
	jmp	short BREAK

ISKANJI:
	mov	CX,1
	jmp	short NEXT

COLON:	cmp	BX,2
	jne	short NEXT		; ':' �擪����2�����ڂ̂Ƃ��̂�
	mov	[BP+idir],BX		;     idir�ɃZ�b�g���A'/'�����ցB
SLASH:	mov	DI,BX			; '/' DI(�t�@�C�����擪�ʒu)�Z�b�g,
	xor	DX,DX			;     DX(�g���q�ʒu)�����B
	jmp	short NEXT

DOT:	mov	DX,BX			; '.' �g���q�ʒu�Z�b�g
	dec	DX
	jmp	short NEXT

	; ������̑��������������B
	;  SI:   ������̐擪�A�h���X
	;  idir: �h���C�u��������Ƃ��͂��̒���(2)�������Ă���,�Ȃ����0�B
	;  DI:   �t�@�C�����擪�̈ʒu�������Ă���
	;  DX:   �g���q('.')�̈ʒu�������Ă���B�Ȃ����0�B
	; �ʒu�͕�����擪����̋����B
BREAK:
	test	DX,DX
	jz	short NO_DOT	; '.'���Ȃ��Ƃ��� DX�ɕ����񖖔��ʒu�������
	mov	AX,SI
	add	SI,DI		; path+ibase
	cmp	byte ptr [SI],'.'
	mov	SI,AX
	jne	short L2	; '.'���t�@�C�����擪�ɂȂ����skip
	mov	CX,BX
	dec	CX
	cmp	CX,DX
	jne	short L2	; '.'���t�@�C�����擪�ƕ����񖖔��ɂ���Ȃ�A
IF 0	; �ȉ��𖳌��ɂ���B���Ȃ킿���O�̗��[��'.'�Ȃ�t�@�C���������ɂ���B
	mov	DI,BX		;    �t�@�C�����擪�ʒu�𖖔�(=�Ȃ�)�ɁB
ENDIF
NO_DOT:	mov	DX,BX
L2:

	CLD
	s_push	DS
	s_pop	ES

	mov	AX,DI			; ibase

	mov	CX,[BP+idir]
	_les	DI,[BP+drv]
    l_ <or	[BP+drv+2],DI>
    s_ <test	DI,DI>
	jz	short NO_DRV
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_DRV:
	add	SI,CX

	mov	CX,AX
	sub	CX,[BP+idir]
	_les	DI,[BP+dir]
    l_ <or	[BP+dir+2],DI>
    s_ <test	DI,DI>
	jz	short NO_DIR
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_DIR:
	add	SI,CX

	_les	DI,[BP+base]
    l_ <or	[BP+base+2],DI>
    s_ <test	DI,DI>
	jz	short NO_BASE
	mov	CX,DX
	sub	CX,AX
	cmp	CX,8
	jle	short BASE8
	mov	CX,8
BASE8:	add	AX,CX
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_BASE:
	add	SI,DX
	sub	SI,AX

	_les	DI,[BP+ext]
	_mov	AX,ES
    l_ <or	AX,DI>
    s_ <test	DI,DI>
	jz	short NO_EXT
	movsw
	movsw
	mov	byte ptr ES:[DI],0
NO_EXT:

	_pop	DS
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	DATASIZE*2*5
endfunc		; }

END
