; master library - vtext
;
; Description:
;	�e�L�X�g��ʂ� EMS�܂��̓��C���������ɑޔ��C��������
;
; Function/Procedures:
;	int vtext_backup( int use_main ) ;	(�ޔ�)
;	int vtext_restore( void ) ;		(����)
;
; Parameters:
;	int use_main	0 = EMS�̂�, 1 = EMS�Ŏ��s�����烁�C��������
;
; Returns:
;	int	1 = ����
;		0 = ���s( EMS�����݂��Ȃ��AEMS,���C��������������Ȃ��Ȃ� )
;		    (���ɑҔ����Ă���̂łȂ���΁A�������Ȃ��Ă�
;		     ��ʃ��[�h�Ȃǂ͑Ҕ����Ă���̂ŁArestore�͉\)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: i8086
;
; Notes:
;	���ɑޔ����Ă���̂ɂ�����x�ޔ����悤�Ƃ���ƃG���[�ɂȂ�܂��B
;	�����l�ɁA�J�����A���Q����s�͂ł��܂���B
;	�ޔ��A�J���ƌ��݂Ȃ�Ή���ł����s�ł��܂��B
;
;	�E��ʃ��[�h�AVRAM���e�A�J�[�\���ʒu�ƌ`���Ҕ��^�������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����: Mikio( strvram.c )
;	�ύX�E����(asm): ���ˏ��F
;	�ύX(DOS/V��): �̂�/V
;
; Revision History:
;	92/12/02 Initial
;	92/12/05 bugfix
;	92/12/28 text_restore��VRAM���e�����O�Ɋe��p�����[�^�𕜌�����悤��
;	93/ 3/23 �������}�l�[�W���g�p
;	93/11/16 DOS/V ��
;	94/ 4/ 9 Initial: vtbackup.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 2/23 [M0.22k] EMS�Ȃ���EMS����Ȃ��w��̂Ƃ������[�h�Ƃ��͕ۑ�
;	95/ 3/ 3 [M0.22k] vtext_restore: backup���Ɠ����r�f�I���[�h�Ȃ�
;			  �r�f�I���[�h�̍Đݒ�͏ȗ�����悤�ɂ���


	.MODEL SMALL
	include func.inc
	include vgc.inc
	include super.inc

	EXTRN	GET_MACHINE:CALLMODEL

	EXTRN	EMS_EXIST:CALLMODEL
	EXTRN	EMS_ALLOCATE:CALLMODEL
	EXTRN	EMS_FREE:CALLMODEL
	EXTRN	EMS_READ:CALLMODEL
	EXTRN	EMS_WRITE:CALLMODEL

	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramSize : WORD
	EXTRN VTextState:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

backup_handle_seg dw	0	; EMS Handle / Main Memory Segment
backup_type	  db	0	; 0 = �o�b�N�A�b�v����ĂȂ�, 1=MAIN, 2=EMS

BACKUP_TYPE_MAIN	equ 1
BACKUP_TYPE_EMS		equ 2
BACKUP_TYPE_MODE	equ 3

	.DATA?
vbackup_curx	dw	?
vbackup_cury	dw	?
vbackup_stat	VIDEO_STATE <?>
vbackup_stat2	VIDEO_STATE <?>
backup_sysline	dw	?
cursor_shape	dw	?

	.CODE

	EXTRN	VTEXT_GETCURPOS:CALLMODEL
	EXTRN	VTEXT_LOCATE:CALLMODEL
	EXTRN	VTEXT_GETCURSOR:CALLMODEL
	EXTRN	VTEXT_SETCURSOR:CALLMODEL
	EXTRN	VTEXT_SYSTEMLINE_SHOWN:CALLMODEL
	EXTRN	VTEXT_SYSTEMLINE_SHOW:CALLMODEL
	EXTRN	VTEXT_SYSTEMLINE_HIDE:CALLMODEL
	EXTRN	BACKUP_VIDEO_STATE:CALLMODEL
	EXTRN	RESTORE_VIDEO_STATE:CALLMODEL

func VTEXT_BACKUP	; vtext_backup() {
	push	BP
	mov	BP,SP
	mov	BP,[BP+(RETSIZE+1)*2]	; use_main
	neg	BP
	sbb	BP,BP		; if use_main != 0 then BP=ffff else BP=0

	CLD

	xor	AX,AX
	cmp	backup_type,BACKUP_TYPE_MODE
	je	short B_GO
	cmp	backup_type,AL
	jne	B_NG		; ���łɑޔ����Ă�Ȃ�G���[
B_GO:

	call	GET_MACHINE		; foolproof

	call	EMS_EXIST
	dec	AX
	js	short B_TRY_MAIN

	xor	AX,AX
	push	AX
	mov	AX,TextVramSize
	add	AX,AX
	push	AX
	call	EMS_ALLOCATE
	test	AX,AX
	jz	short B_TRY_MAIN

	mov	backup_handle_seg,AX

	push	AX	; handle
	xor	AX,AX
	push	AX	; write offset
	push	AX	;
	les	BX,TextVramAdr	; text seg
	push	ES		;
	push	BX		; text offset
	push	AX			; size
	mov	AX,TextVramSize
	add	AX,AX
	push	AX			;
	call	EMS_WRITE
	dec	AX
	mov	AL,BACKUP_TYPE_EMS
	js	short B_OK

	push	backup_handle_seg
	call	EMS_FREE

B_TRY_MAIN:
	xor	AX,AX
	mov	backup_handle_seg,AX
	inc	BP
	mov	AL,BACKUP_TYPE_MODE
	jnz	short B_MODEBACKUP_ONLY
	mov	AX,TextVramSize
	shr	AX,1		; /2
	shr	AX,1		; /4
	shr	AX,1		; /8
	push	AX
	mov	mem_AllocID,MEMID_textback
	call	HMEM_ALLOC
	jc	short B_NG

	mov	backup_handle_seg,AX
	mov	ES,AX
	push	DS
	push	SI
	push	DI
	mov	CX,TextVramSize
	lds	SI,TextVramAdr
	xor	DI,DI
	rep	movsw
	pop	DI
	pop	SI
	pop	DS
	mov	AL,BACKUP_TYPE_MAIN
B_MODEBACKUP_ONLY:

B_OK:
	mov	backup_type,AL
	_call	VTEXT_SYSTEMLINE_SHOWN
	mov	backup_sysline,AX
	_call	VTEXT_GETCURPOS
	mov	vbackup_curx,AX
	mov	vbackup_cury,DX

	_call	VTEXT_GETCURSOR
	mov	cursor_shape,AX

	_push	DS
	mov	AX,offset vbackup_stat
	push	AX
	_call	BACKUP_VIDEO_STATE
	mov	AX,0
	cmp	backup_type,BACKUP_TYPE_MODE
	je	short B_NG
	inc	AX
B_NG:
	pop	BP
	ret	2
endfunc			; }

func VTEXT_RESTORE	; vtext_restore() {
	push	DI
	cmp	backup_type,0
	je	short R_FAULT

	CLD

	_push	DS
	mov	AX,offset vbackup_stat2
	push	AX
	call	BACKUP_VIDEO_STATE

	; VIDEO_STATE ��4�o�C�g�ɉ���
	mov	AX,word ptr vbackup_stat
	cmp	AX,word ptr vbackup_stat2
	jne	short DO_RESTORE_MODE
	mov	AX,word ptr vbackup_stat+2
	cmp	AX,word ptr vbackup_stat2+2
	je	short SKIP_RESTORE_MODE

DO_RESTORE_MODE:
	_push	DS
	mov	AX,offset vbackup_stat
	push	AX
	call	RESTORE_VIDEO_STATE
SKIP_RESTORE_MODE:

	cmp	backup_sysline,0
	je	short R_SOFF
	_call	VTEXT_SYSTEMLINE_SHOW
	jmp	short R_START
R_SOFF:
	_call	VTEXT_SYSTEMLINE_HIDE

R_START:
	push	cursor_shape
	call	VTEXT_SETCURSOR

	push	vbackup_curx
	push	vbackup_cury
	call	VTEXT_LOCATE

	les	DI,TextVramAdr
	mov	AX,backup_handle_seg

	cmp	backup_type,BACKUP_TYPE_EMS
	je	short R_EMS

R_MAIN:
	test	AX,AX
	jz	short R_END
	push	AX		; <--- HMEM_FREE's argument
	push	DS
	push	SI
	mov	CX,TextVramSize
	mov	DS,AX
	xor	SI,SI
	rep	movsw
	pop	SI
	pop	DS
	call	HMEM_FREE
	jmp	short R_END

R_FAULT:
	xor	AX,AX
	pop	DI
	ret

R_EMS:
	push	AX		; push handle

	xor	AX,AX
	push	AX		; push 0L
	push	AX		; 

	push	ES		; push TextVramSeg:0
	push	DI		;

	push	AX		; 
	mov	AX,TextVramSize	; push (long)TVRAMSIZE
	add	AX,AX
	push	AX		; 
	call	EMS_READ
	or	AX,AX
	jnz	short R_FAULT
	push	backup_handle_seg
	call	EMS_FREE

R_END:
	test	byte ptr VTextState,01
	jne	short NO_REF
	mov	CX,TextVramSize
	les	DI,TextVramAdr
	mov	AH,0ffh
	int	10h
NO_REF:
	and	VTextState,not 8000h	; text mode

	xor	AX,AX
	mov	backup_type,AL
	inc	AX			; AX = 1
	pop	DI
	ret
endfunc			; }

END
