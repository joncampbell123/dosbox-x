; master library - MS-DOS
;
; Description:
;	path ��4�̍\���v�f�ɕ�������( \ -> /�ϊ��� )
;
; Functions/Procedures:
;	void file_splitpath_slash(char *path,char *drv,char *dir,char *name,char *ext);
;
; Parameters:
;	path	�������������p�X��( e.g. "A:\HOGE\ABC.C" )	NULL�֎~
;	drv	�h���C�u�����̊i�[��( "A:" )			NULL��
;	dir	�p�X�����̊i�[��( "/HOGE/" )			NULL��
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
;
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
;	�a�����
;	����
;
; Revision History:
;	94/ 5/29 Initial: filsplsl.asm/master.lib 0.23
;	95/ 3/10 [M0.22k] NULL�̈��������S��NULL�s�������̂����

	.MODEL SMALL
	include	func.inc

EOS	equ	0	;C������̏I�[����

if DATASIZE eq 2
	_seg	equ	ES
else
	_seg	equ	DS
endif

; COPYSTR arg
; in:
;  arg   (ES:)DI�ɂ����f�[�^
;  SI    �ǂݎ�茳
;  CX    �]������������
; out:
;  (ES:)DI��NULL�Ȃ�΁ASI,CX�͂��̂܂܁B
;  NULL�łȂ���΁AES:DI�ɕ����񂪕��ʂ���ASI,DI��CX�����Z����ACX=0�ɂȂ�B
;
;  AX�j��
COPYSTR macro arg
	local SKIP
	_les	DI,arg
	_mov	AX,ES
    l_ <or	AX,DI>
    s_ <test	DI,DI>
	jz	short SKIP
	rep	movsb
	mov	byte ptr _seg:[DI],EOS
SKIP:
endm

	.CODE

func	FILE_SPLITPATH_SLASH	; file_splitpath_slash() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	path	= (RETSIZE + 1 + DATASIZE*4)*2
	drv	= (RETSIZE + 1 + DATASIZE*3)*2
	dir	= (RETSIZE + 1 + DATASIZE*2)*2
	_name	= (RETSIZE + 1 + DATASIZE*1)*2
	ext	= (RETSIZE + 1 + DATASIZE*0)*2

	s_mov	AX,DS
	s_mov	ES,AX

;�f�B���N�g����؂�� '/' �ɕϊ�����
;1�o�C�g�ڂ� 0x81-0x9f,0xe0-0xef �Ȃ�΁A�V�t�gJIS�����ł���Ƃ��Ă���
	_lds	SI,[BP+path]
transloop:
	lodsb
	or	AL,EOS
	jz	short transend
	cmp	AL,'\'
	jne	short nottrans
	mov	byte ptr [SI - 1],'/'
nottrans:
	cmp	AL,081h
	jb	short transloop
	cmp	AL,09fh
	jbe	short skipbyte
	cmp	AL,0e0h
	jb	short transloop
	cmp	AL,0efh
	jnbe	short transloop
skipbyte:
	lodsb
	or	AL,EOS
	jnz	short transloop
transend:
	mov	BX,SI	;BX = ������ + 1

;�h���C�u���𓾂�A2�����ȉ��Ȃ�h���C�u���͑��݂��Ȃ�
	mov	CX,0
	sub	SI,[BP+path]		; SI=����
	cmp	SI,2
	mov	SI,[BP+path]		; SI=���p�X�擪
	jb	short drvend
	cmp	byte ptr [SI + 1],':'
	jne	short drvend
	mov	CX,2
drvend:
	COPYSTR	[BP+drv]
	add	SI,CX

;�f�B���N�g�����𓾂�
	_mov	AX,DS		;scas����
	_mov	ES,AX
	lea	DI,[BX - 1]	;DI = ������
	mov	CX,BX
	sub	CX,SI		;CX = ������

	; '/'�𕶎��񖖔����猟������
 	xor	AX,AX		;
	or	AL,'/'		;with ZF = 0
	STD			;�f�N�������g����
	repne	scasb
	CLD			;�����߂�
	jne	short NO_DIR	; ������Ȃ���� CX = 0 �ɂȂ��Ă�
	inc	CX		; ����������CX = len+1('/'���܂܂��邽��)
NO_DIR:
	COPYSTR	[BP+dir]
	add	SI,CX

;�t�@�C�����𓾂�
;CX = �����񒷁A���� -> CX = �g���q���A�� DX-CX = �t�@�C������
	_mov	AX,DS		;scas����
	_mov	ES,AX
	mov	CX,BX
	sub	CX,SI		;
	mov	DX,CX		;DX = CX = ������
	mov	DI,SI
	xor	AX,AX		;
	or	AL,'.'		;with ZF = 0
	repne	scasb
	jne	short noext
	inc	CX
noext:
	sub	DX,CX
	xchg	DX,CX
	COPYSTR	[BP+_name]
	add	SI,CX

	mov	CX,DX
	cmp	CX,4
	jle	short do_ext
	mov	CX,4		; �g���q��4�������~�b�g��������
do_ext:
	COPYSTR	[BP+ext]

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	DATASIZE*10
endfunc			; }

END
