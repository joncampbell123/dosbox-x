; master library - font - cgrom - vsync
;
; Description:
;	VSYNC���荞�݂��g�p���Ĕ��p�t�H���g��ǂݍ���
;
; Function/Procedures:
;	int font_entry_cgrom( int firstchar, int lastchar ) ;
;
; Parameters:
;	firstchar	�ǂݍ��ލŏ��̃L�����N�^�R�[�h(0�`255)
;	lastchar	�ǂݍ��ލŌ�̃L�����N�^�R�[�h(0�`255)
;
; Returns:
;	NoError			(cy=0)	����
;	InsufficientMemory	(cy=1)	�������s��
;	InvalidData		(cy=1)	�T�C�Y��8x16dot����Ȃ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal Mode
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���s�O�� vsync_start()�����s����K�v������܂��B
;	�I����v���O�����I���O�� vsync_end()�����s����K�v������܂��B
;
;	firstchar > lastchar �̏ꍇ�A�������܂���B
;	font_ReadChar �� firstchar���Afont_ReadEndChar �� lastchar��
;	���̂܂ܓ]�ʂ���ċN�����܂��B
;	font_ReadChar�́A1�����ǂݍ��ޖ��� 1�����܂��B
;
;	���̓ǂݍ��ݏ����������������ǂ����𔻒肷��ɂ́A
;	�O���[�o���ϐ� font_ReadChar �� font_ReadEndChar ���傫�����ǂ���
;	(�傫����Ί���)�Ŕ��f���܂��B
;
;	�@���s�O�� vsync_proc_set ���ꂽ���͕̂�������Ȃ��̂ŁA�I���𔻒f
;	������A�ēx vsync_proc_set ���ĕ������Ă��������B
;
;	�@vsync�P���1���������ǂݍ��ނ̂ŁA256�����ǂނƂ��Ȃ莞�Ԃ�
;	������܂��B�ł��邾���A�K�v�ŏ����͈̔͂��w�肷��悤�ɂ��ĉ������B
;
;	�@�܂��A�v���O�����N�����߂݂̂��̏��������s���ăt�@�C���ɕۑ����A
;	2��ڈȍ~�� font_read_bfnt ��p����̂������I�ł��B
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
;	93/11/20 Initial: fontentc.asm/master.lib 0.21
;	94/ 1/26 [M0.22] font_AnkSize�`�F�b�N
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/30 [M0.22k] font_AnkPara���ݒ�

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	HMEM_ALLOC:CALLMODEL

	.DATA
	EXTRN	font_AnkSeg:WORD		; font.asm
	EXTRN	font_AnkSize:WORD		; font.asm
	EXTRN	font_AnkPara:WORD		; font.asm
	EXTRN	font_ReadChar:WORD		; fontcg.asm
	EXTRN	font_ReadEndChar:WORD		; fontcg.asm
	EXTRN	vsync_Proc : DWORD		; vs.asm
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
font_read_proc proc far
	mov	AX,font_ReadChar
	cmp	AX,font_ReadEndChar
	jg	short READ_RET
	mov	BX,font_AnkSeg
	add	BX,AX
	mov	ES,BX
	xor	BX,BX
	mov	CX,AX

	mov	AL,0bh
	out	68h,AL		; CG ROM dot access

	jmp	$+2
	mov	AL,0
	out	0a1h,AL
	jmp	$+2
	jmp	$+2
	mov	AL,CL
	out	0a3h,AL		; set character code

READLOOP:
	mov	AL,BL
	out	0a5h,AL
	jmp	$+2
	jmp	$+2
	in	AL,0a9h
	mov	ES:[BX],AL
	inc	BL
	cmp	BL,16
	jl	short READLOOP

	mov	AL,0ah
	out	68h,AL		; CG ROM code access

	inc	font_ReadChar
READ_RET:
	retf
font_read_proc endp

func FONT_ENTRY_CGROM ; font_entry_cgrom() {
	cmp	font_AnkSize,0
	je	short GO
	cmp	font_AnkSize,0110h
	mov	AX,InvalidData
	stc
	jne	short IGNORE
GO:

	push	BP
	mov	BP,SP
	; ����
	firstchar = (RETSIZE+2)*2
	lastchar = (RETSIZE+1)*2

	mov	AX,[BP+firstchar]
	mov	BX,[BP+lastchar]
	mov	font_readchar,AX
	mov	font_readendchar,BX

	mov	DX,font_AnkSeg
	test	DX,DX
	jnz	short CONT
	; font_AnkSeg���ݒ肳��ĂȂ��Ƃ�
	mov	mem_AllocID,MEMID_font
	mov	AX,256		; 256char * 16 bytes
	push	AX
	call	HMEM_ALLOC
	jc	short DAME
	mov	font_AnkSeg,AX
	mov	font_AnkPara,1
	mov	font_AnkSize,0110h
	mov	DX,AX
DAME:
	mov	AX,InsufficientMemory
	jc	short ENOMEM
CONT:
	pushf
	CLI
	mov	WORD PTR vsync_Proc,offset font_read_proc
	mov	WORD PTR vsync_Proc + 2,CS
	popf

	xor	AX,AX	; NoError
ENOMEM:
	pop	BP
IGNORE:
	ret	4
endfunc		; }

END
