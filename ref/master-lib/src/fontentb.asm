; superimpose & master library module
;
; Description:
;	BFNT��ANK�t�@�C����ǂݎ��A�L������
;
; Funcsions/Procedures:
;	int font_entry_bfnt( char * filename ) ;
;
; Returns:
;	���̂ǂꂩ�B
;		NoError			����
;		FileNotFound		�t�@�C�����Ȃ�
;		InvalidData		�t�@�C���`���╶���̑傫���������Ȃ�
;		InsufficientMemory	������������Ȃ�
;
; Notes:
;	Heap����256�����Ԃ�ɕK�v�ȃ��������擾���܂��B(8x16dot�Ȃ�4KB)
;	�����̑傫���́Axdots��8�̔{���ł��A(xdots/8 * ydots)��16�̔{����
;	�Ȃ���� InvalidData�Ƃ��Ĕ����܂��B
;	super.lib 0.22b �� ank_font_load()�ɑΉ����܂��B
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;	$Id: ankload.asm 0.03 93/01/16 14:16:40 Kazumi Rel $
;	93/ 2/24 Initial: master.lib <- super.lib 0.22b
;	93/ 8/ 7 [M0.20] �g���w�b�_�Ή�
;	94/ 6/10 [M0.23] BUGFIX �G���[���t�@�C������ĂȂ�����
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/14 [M0.22k] BUGFIX ���T�C�Y�̃f�[�^�����ɂ���Ƃ��Ɏ���ł�

	.MODEL	SMALL
	include	super.inc
	include func.inc
	EXTRN	FONTFILE_OPEN:CALLMODEL
	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL
	EXTRN	BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_SKIP:CALLMODEL

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
func FONT_ENTRY_BFNT	; font_entry_bfnt() {
	push	BP
	mov	BP,SP
	sub	SP,2+BFNT_HEADER_SIZE

	; ����
	filename = (RETSIZE+1)*2

	; ���[�J���ϐ�
	handle	= (-2)
	header	= (-2-BFNT_HEADER_SIZE)

	_push	[BP+filename+2]
	push	[BP+filename]
	_call	FONTFILE_OPEN
	jc	short ERROR_QUIT2
	mov	[BP+handle],AX

	push	AX
	lea	CX,[BP+header]
	push	AX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	_call	BFNT_HEADER_READ
	pop	BX
	jc	short ERROR_QUIT

	lea	CX,[BP+header]
	push	BX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	_call	BFNT_EXTEND_HEADER_SKIP
	jc	short ERROR_QUIT

	mov	AL,[BP+header].col
	cmp	AL,0			; 2�F�p���b�g�Ȃ��łȂ���΂Ȃ�Ȃ�
	jne	short READ_ERROR

	mov	AH,byte ptr [BP+header].Xdots
	add	AH,7
	mov	CL,3
	shr	AH,CL			; Xdots�� 8�Ŋ���
	mov	AL,byte ptr [BP+header].Ydots
	test	AX,AX
	mov	DX,AX
	jz	short READ_ERROR
	cmp	font_AnkSeg,0
	je	short FORCE_ALLOC

	cmp	DX,font_AnkSize		; ���݂Ɠ����T�C�Y�ł���Ώ㏑��
	mov	AX,font_AnkSeg
	je	short SKIP_ALLOCATE
	mov	AX,DX

	; in: AX=DX=size
FORCE_ALLOC:
	mul	AH
	test	AX,0f00fh		; 16�̔{���łȂ���Αʖ�,
					; 256������64K�z����̂��ʖ�
	jnz	short READ_ERROR
	mov	CL,4
	shr	AX,CL
	mov	font_AnkPara,AX		; 1�����ɕK�v�ȃp���O���t��
	xchg	DX,AX
	mov	font_AnkSize,AX		; 1�����̑傫��
					; DX = font_AnkPara

	mov	AX,font_AnkSeg	; ���łɊm�ۂ���Ă���Ȃ�J������
	test	AX,AX
	jz	short ALLOC
	push	AX
	_call	HMEM_FREE
	jmp	short ALLOC

	; ����ȂƂ��Ɂ`�`�`
MEMORY:
        mov     AX,InsufficientMemory
        jmp	short ERROR_QUIT
READ_ERROR:
	mov	AX,InvalidData
ERROR_QUIT:
	push	AX
	mov	BX,[BP+handle]
	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h
	pop	AX
ERROR_QUIT2:
	stc
	jmp	short RETURN
	EVEN
ALLOC:
	mov	AH,DL	; AX=font_AnkPara * 256
	mov	mem_AllocID,MEMID_font
	mov	AL,0
	push	AX
	_call	HMEM_ALLOC
	jc	short MEMORY
	mov	font_AnkSeg,AX

	; in: AX=font_AnkSeg
SKIP_ALLOCATE:
	push	DS

	mov	BX,AX
	mov	AX,[BP+header].START
	mov	CX,[BP+header].END_
	sub	CX,AX
	inc	CX			; CX=������
	mov	AH,0			; AX=�ŏ��̕����ԍ�
	mul	font_AnkPara
	add	BX,AX
	mov	AX,font_AnkPara
	mov	DS,BX

	mul	CX	; AX=�������Ԃ�ɕK�v�ȃp���O���t��
	mov	CL,4
	shl	AX,CL
	mov	CX,AX	; CX = �������Ԃ�ɕK�v�ȃo�C�g��
	xor	DX,DX
	mov	BX,[BP+handle]
	mov	AH,3fh		; DOS: READ HANDLE
	int	21h
	pop	DS
	jc	short READ_ERROR

	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h

	mov	AX,NoError
	clc
RETURN:
	mov	SP,BP
	pop	BP
	ret	DATASIZE*2
endfunc			; }

END
