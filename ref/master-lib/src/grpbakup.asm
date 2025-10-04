; master library - PC98V
;
; Description:
;	�O���t�B�b�N���(�\��)�� EMS�ɑޔ��C��������
;
; Function/Procedures:
;	int graph_backup(int pagemap) ;	(�ޔ�)
;	int graph_restore(void) ;	(����)
;
; Parameters:
;	int pagemap	(pagemap & 1) != 0 �ŏ��̃y�[�W��ޔ�
;			(pagemap & 2) != 0 �Q�Ԗڂ̕ł�ޔ�
;
; Returns:
;	int	1 = ����
;		0 = ���s( EMS�����݂��Ȃ��A����Ȃ��Ȃ� )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRCG
;
; Notes:
;	GRCG�̊e���W�X�^�̓��e���j�󂳂�܂��B
;	�A�N�Z�X�y�[�W�́Apage 0�ɂȂ�܂��B
;	EMS�h���C�o�́AMelware, NEC, I�O DATA�œ���m�F���Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����: Mikio( strvram.c )
;	�ύX�E����(asm): ���ˏ��F
;
; Revision History:
;	92/12/02 Initial
;	92/12/05 graph_restore() bugfix
;	92/12/28 graph_backup()�Ɉ���(�Ŏw��)��ǉ�
;	93/ 1/26 large�� bugfix
;	95/ 3/20 [M0.22k] BUGFIX GRCG��OFF�ɂ��Ă��Ȃ�����

	.186
	.MODEL SMALL
	include func.inc
	EXTRN	EMS_EXIST:CALLMODEL
	EXTRN	EMS_ALLOCATE:CALLMODEL
	EXTRN	EMS_FREE:CALLMODEL
	EXTRN	EMS_MOVEMEMORYREGION:CALLMODEL
	EXTRN	EMS_ENABLEPAGEFRAME:CALLMODEL

	.DATA
vram_handle	dw	0

	.DATA?
page_map	dw	?

	.CODE

EMSSTRUCT struc
region_length		dd ?
source_memory_type	db ?
source_handle		dw ?
source_initial_offset	dw ?
source_initial_seg_page	dw ?
dest_memory_type	db ?
dest_handle		dw ?
dest_initial_offset	dw ?
dest_initial_seg_page	dw ?
EMSSTRUCT ends

GC_MODEREG equ	7ch
GC_TILEREG equ	7eh
GC_RMW	equ	0c0h
GC_TCR	equ	080h
GC_B	equ	1110b
GC_R	equ	1101b
GC_G	equ	1011b
GC_I	equ	0111b

GRAM_APAGE equ 0a6h


MRETURN macro
	pop	DI
	pop	SI
	leave
	ret 2
	EVEN
	endm

func GRAPH_BACKUP	; {
	enter 18,0
	push	SI
	push	DI

	; ����
	pagemap	= (RETSIZE+1)*2

	; local variables
	trans	= -18

	mov	SI,[BP+pagemap]		; SI = pagemap
	and	SI,3
	jz	short BAK_ERROR
	mov	page_map,SI
	cmp	vram_handle,0
	je	short BAK_SKIP
BAK_ERROR:
	xor	AX,AX
	MRETURN

BAK_SKIP:
	call	EMS_EXIST
	or	AX,AX
	je	short BAK_ERROR

	mov	AX,2	; 128KB
	cmp	SI,3
	jl	short BAK_1PAGE
	shl	AX,1	; 256KB
BAK_1PAGE:
	push	AX
	push	0
	call	EMS_ALLOCATE
	mov	vram_handle,AX

	or	AX,AX
	je	short BAK_ERROR

	mov	word ptr [BP+trans].region_length,8000h
	mov	word ptr [BP+trans].region_length+2,0

	mov	[BP+trans].source_memory_type,0	; from Main Memory

	mov	[BP+trans].source_initial_seg_page,0a800H

	mov	[BP+trans].dest_memory_type,1	; to EMS

	mov	AX,vram_handle
	mov	[BP+trans].dest_handle,AX

	xor	AX,AX
	mov	[BP+trans].source_handle,AX
	mov	[BP+trans].source_initial_offset,AX
	mov	[BP+trans].dest_initial_offset,AX
	mov	[BP+trans].dest_initial_seg_page,AX

	xor	DI,DI

	; �y�[�W�̂�[��
BAK_LOOP:
	shr	SI,1
	jnb	short BAK_SKIPPAGE
	mov	AX,DI
	out	GRAM_APAGE,AL

	xor	AX,AX
	out	GC_MODEREG,AL		; GRCG OFF

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AL,GC_TCR or GC_R
	out	GC_MODEREG,AL
	STI
	mov	AL,0ffh
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AX,GC_TCR or GC_G
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AX,GC_TCR or GC_I
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2
BAK_SKIPPAGE:
	inc	DI
	or	SI,SI
	jnz	short BAK_LOOP

	push	SI	; 0
	call	EMS_ENABLEPAGEFRAME

	xor	AX,AX
	out	GRAM_APAGE,AL
	out	GC_MODEREG,AL		; GRCG OFF

	inc	AX			; AX = 1
	jmp	short BAK_RET

BAK_FAILURE:
	push	vram_handle		; �r���Ŏ��s���Ă�����J������
	call	EMS_FREE

	xor	AX,AX
	mov	vram_handle,AX
	out	GC_MODEREG,AL		; GRCG OFF
BAK_RET:
	MRETURN
endfunc	; }


MRETURN macro
	pop	DI
	pop	SI
	leave
	ret
	EVEN
	endm

func GRAPH_RESTORE	; {
	enter 18,0
	push	SI
	push	DI

	; local variables
	trans = -18

	cmp	vram_handle,0
	je	short RES_ERROR
	cmp	page_map,0
	jne	short RES_START
RES_ERROR:
	xor	AX,AX
	MRETURN
RES_START:

	mov	WORD PTR [BP+trans].region_length,8000H
	mov	WORD PTR [BP+trans].region_length+2,0

	mov	[BP+trans].source_memory_type,1

	mov	AX,vram_handle
	mov	[BP+trans].source_handle,AX

	xor	AX,AX
	mov	[BP+trans].dest_memory_type,AL

	mov	[BP+trans].source_initial_offset,AX
	mov	[BP+trans].source_initial_seg_page,AX
	mov	[BP+trans].dest_handle,AX
	mov	[BP+trans].dest_initial_offset,AX

	mov	[BP+trans].dest_initial_seg_page,0a800H

	mov	DI,page_map
	xor	SI,SI

	shr	DI,1
	cmc
	mov	AX,SI
	adc	AX,AX
	out	GRAM_APAGE,AL	; first page

	; �y�[�W�̂�[��
RES_LOOP:
	mov	AL,GC_TCR		; GRCG TCR�őS�� 0 �Ɂ`
	CLI
	out	GC_MODEREG,AL
	STI
	xor	AX,AX
	mov	DX,GC_TILEREG
	out	DX,AL
	out	DX,AL
	out	DX,AL
	out	DX,AL

	push	DI
	mov	BX,0a800H
	mov	CX,8000H/2
	mov	DI,AX
	mov	ES,BX
	rep	stosw
	pop	DI

	out	GC_MODEREG,AL		; GRCG OFF

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	CLI				; GRCG�^�C���̑S�r�b�g ON
	mov	AL,GC_RMW or GC_R
	out	GC_MODEREG,AL
	STI
	mov	AL,0ffh
	mov	DX,GC_TILEREG
	out	DX,AL
	out	DX,AL
	out	DX,AL
	out	DX,AL

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	CLI
	mov	AL,GC_RMW or GC_G
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	;CLI
	mov	AL,GC_RMW or GC_I
	out	GC_MODEREG,AL			; GRCG SETMODE
	;STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2
RES_SKIPPAGE:
	mov	AL,1
	out	GRAM_APAGE,AL

	dec	DI
	jns	short RES_LOOP

	xor	AX,AX
	out	GC_MODEREG,AL			; GRCG OFF

	push	AX
	call	EMS_ENABLEPAGEFRAME

	xor	AX,AX
	out	GRAM_APAGE,AL

	push	vram_handle
	call	EMS_FREE

	mov	vram_handle,0

	neg	SI
	sbb	AX,AX
	inc	AX	; 1 = success, 0 = fault
	MRETURN
endfunc	; }

END
