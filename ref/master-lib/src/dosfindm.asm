; master library - MSDOS
;
; Description:
;	��v����t�@�C���̕�������
;
; Function/Procedures:
;	unsigned dos_findmany( const char far * path, int attribute, struct master_find_t far * buffer, unsigned max_dir ) ;
;
; Parameters:
;	path       �����p�X��
;	attribute  ��������
;	buffer     �i�[��
;	max_dir    �ő��, 1�ȏ�B 0��1�ɕ␳���Ď��s����B
;
; Returns:
;	int	1�ȏ�	buffer�ɏ������񂾌�
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
;	�@DTA�̃A�h���X�ύX���Ȃ��̂ŁADTA�̒��g���j�󂳂�܂���
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 8/23 Initial: dosfintm.asm/master.lib 0.21
;
	.MODEL SMALL
	include func.inc
	.CODE

; struct master_find_t {
;	unsigned long time ;
;	unsigned long size ;
;	char name[13] ;
;	char attribute ;
; }

func DOS_FINDMANY	; dos_findmany() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; ����
	findpath  = (RETSIZE+5)*2
	attribute = (RETSIZE+4)*2
	buffer    = (RETSIZE+2)*2
	max_dir   = (RETSIZE+1)*2

	mov	CX,[BP+attribute]
	lds	DX,[BP+findpath]
	mov	AH,4eh		; findfirst
	int	21h
	mov	DX,0			; ��
	jc	short OWARI

	mov	AH,2fh
	int	21h			; DS:SI=dta
	push	ES
	pop	DS
	lea	SI,[BX+21]		; attrib
	les	DI,[BP+buffer]

	mov	BX,[BP+max_dir]		; BX = �ő吔
READLOOP:
	;�]��
	inc	SI	; skip attribute
	mov	CX,2+2+4+13
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	sub	SI,2+2+4+13+1 ; rewind
	mov	AL,[SI]	; get attribute
	stosb

	inc	DX
	cmp	DX,BX
	jge	short OWARI

	mov	AH,4fh		; findnext
	int	21h
	jnc	short READLOOP
OWARI:
	mov	AX,DX

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	12
endfunc			; }

END
