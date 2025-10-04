; superimpose & master library module
;
; Description:
;	BFNT�t�@�C����������p�^�[�����ꊇ�ݒ�
;
; Functions/Procedures:
;	int super_change_erase_bfnt( int patnum, const char * filename ) ;
;
; Parameters:
;	patnum    �ύX�����ŏ��̓o�^����Ă���p�^�[���ԍ�
;	filename  BFNT�t�@�C����
;
; Returns:
;	NoError       ����
;	FileNotFound  �w�肳�ꂽ BFNT �t�@�C�����I�[�v���ł��Ȃ�
;	InvalidData   �t�H���g�t�@�C�����g���Ȃ��K�i�ł���
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
;	small data model�ł́ADS != SS�̂Ƃ��Ɏg��Ȃ��ł��������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: supercgb.asm 0.03 93/01/15 11:46:10 Kazumi Rel $
; 
;3/10?
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/11/22 [M0.21] bugfix, return size���ς������B����Ⴀ
;	95/ 3/25 [M0.22k] BUGFIX bfnt_change_erase_pat�G���[���ɐ���I�����Ă�
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	FONTFILE_OPEN:CALLMODEL, BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_SKIP:CALLMODEL, BFNT_PALETTE_SKIP:CALLMODEL
	EXTRN	BFNT_CHANGE_ERASE_PAT:CALLMODEL, FONTFILE_CLOSE:CALLMODEL

func SUPER_CHANGE_ERASE_BFNT	; super_change_erase_bfnt() {
	push	BP
	mov	BP,SP
	sub	SP,BFNT_HEADER_SIZE

	patnum	= (RETSIZE+1+DATASIZE)*2
	filename= (RETSIZE+1)*2

	header	= -BFNT_HEADER_SIZE

	_push	[BP+filename+2]
	push	[BP+filename]
	_call	FONTFILE_OPEN
	jc	short OPEN_ERROR

	push	SI
	push	DI

	mov	SI,AX			;file handle
	lea	DI,[BP+header]
	EVEN
	push	SI	; handle
	_push	SS
	push	DI	; header
	_call	BFNT_HEADER_READ
	jc	short ERROR

	mov	AX,[BP+header].hdrSize
	or	AX,AX
	jz	short no_exthdr
	push	SI	; handle
	_push	DS
	push	DI	; header
	_call	BFNT_EXTEND_HEADER_SKIP
	EVEN
no_exthdr:
	test	[BP+header].col,80h	;palette table check
	jz	short palette_end
	push	SI	; handle
	_push	SS
	push	DI	; header
	_call	BFNT_PALETTE_SKIP
	EVEN
palette_end:
	push	[BP+patnum]	; patnum
	push	SI		; handle
	_push	SS
	push	DI		; header
	_call	BFNT_CHANGE_ERASE_PAT	; (patnum, handle, BFNT.. * header)
	jc	short ERROR

	push	SI		; handle
	_call	FONTFILE_CLOSE
	jmp	short RETURN

ERROR:
	push	AX
	push	SI		; handle
	_call	FONTFILE_CLOSE
	pop	AX
	stc

RETURN:
	pop	DI
	pop	SI
OPEN_ERROR:
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
endfunc		; }

END
