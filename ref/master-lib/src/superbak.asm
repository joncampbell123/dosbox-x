; master library - 
;
; Description:
;	�p�^�[����EMS�ւ̕ۑ��ƕ����A�J��
;
; Function/Procedures:
;	int super_backup_ems( unsigned * handle, int first_pat, int last_pat ) ;	�ۑ�
;	int super_restore_ems( unsigned handle, int load_to ) ;	����
;	void super_free_ems( void ) ;				�S�Ă̊J��
;
; Parameters:
;	* handle	�Ҕ������Ǘ��ԍ��̊i�[��
;	first_pat	�Ҕ�����p�^�[���̐擪�ԍ�
;	last_pat	�Ҕ�����p�^�[���̍ŏI�ԍ�
;
;	handle		��������Ǘ��ԍ�(super_backup_ems�œ����l)
;	load_to		�������ʂ̓ǂݍ��ݐ�p�^�[���ԍ�(-1�Ȃ�{���̔ԍ�)
;
; Returns:
;	super_backup_ems:
;	 0			�ۑ������B
;	 InsufficientMemory	EMS������Ȃ�
;	 GeneralFailure		���͈̔͂Ƀf�[�^�͑S���o�^����Ă��Ȃ�
;	 InvalidData		first_pat,last_pat�̒l������
;	super_restore_ems:
;	 NoError		�i�[�����B
;	 InsufficientMemory	�������ʂ��i�[���郁����������Ȃ��B
;	 InvalidData		load_to������
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	186
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	EMS�n���h���͑S�̂ň�Ǘ����Ă���B
;
;	handle�͊Ǘ��u���b�N�̃Z�O�����g�A�h���X�B
;	�Ǘ��u���b�N�̍\���́A�ȉ��̒ʂ�B
;	top_offset	1 DWORD		EMS�������̒��̐擪�I�t�Z�b�g
;	top_number	1 WORD		�擪�̑Ҕ����p�^�[���ԍ�
;	num_data	1 WORD		�Ҕ��p�^�[����
;	patsize[]	n WORD		�Ҕ��p�^�[���̑傫���̔z��
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
;	93/12/19 Initial: superbak.asm/master.lib 0.22
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/23 [M0.22k] super_restore_ems() load_to!=0���Ɉُ킾����

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

IFDEF ??version		; tasm check
	JUMPS
	WARN
ENDIF

	.DATA
	EXTRN super_backup_ems_handle:WORD	; superems.asm
	EXTRN super_backup_ems_pos:DWORD	; superems.asm
	EXTRN super_patdata:WORD		; superpa.asm
	EXTRN super_patsize:WORD		; superpa.asm
	EXTRN super_patnum:WORD			; superpa.asm
	EXTRN mem_AllocID:WORD			; mem.asm

	.CODE
	EXTRN	EMS_EXIST:CALLMODEL		; emsexist.asm
	EXTRN	EMS_ALLOCATE:CALLMODEL		; emsalloc.asm
	EXTRN	EMS_REALLOCATE:CALLMODEL	; emsrealc.asm
	EXTRN	EMS_WRITE:CALLMODEL		; emswrite.asm
	EXTRN	EMS_READ:CALLMODEL		; emsread.asm
	EXTRN	EMS_FREE:CALLMODEL		; emsfree.asm
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL	; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL		; memheap.asm
	EXTRN	SUPER_ENTRY_AT:CALLMODEL	; superat.asm

BACKUP_RECORD	struc
 top_offset	dd	?
 top_number	dw	?
 num_data	dw	?
 patsize	dw	?
BACKUP_RECORD	ends



func SUPER_BACKUP_EMS	; super_backup_ems() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	;
	; ����
	phandle = (RETSIZE+3)*2
	first_pat = (RETSIZE+2)*2
	last_pat = (RETSIZE+1)*2

	_call	EMS_EXIST
	test	AX,AX
	jz	BACKUP_NOMEM

	mov	AX,super_patnum
	mov	CX,[BP+first_pat]	; first_pat�͈̔͌���
	cmp	CX,AX
	jae	BACKUP_INVAL
	cmp	[BP+last_pat],AX	; last_pat�͈̔͌���
	jae	BACKUP_INVAL

	mov	CX,[BP+last_pat]
	sub	CX,[BP+first_pat]
	jc	BACKUP_INVAL		; jump out of range(67)
	inc	CX
CREATE_BLOCK:			; �Ǘ��u���b�N�̊m��
	mov	AX,CX
	shl	AX,1
	add	AX,8		; ��*2+8byte���A�p�^�[���Ǘ��u���b�N�̑傫��
	push	AX
	mov	mem_AllocID,MEMID_super
	_call	HMEM_ALLOCBYTE
	jc	BACKUP_NOMEM		; jump out of range(60)
	s_mov	BX,DS
	s_mov	ES,BX
	_les	DI,[BP+phandle]
	CLD
	stosw

	; �Ǘ��u���b�N�ւ̏�������
	mov	ES,AX
	xor	DI,DI
	mov	AX,word ptr super_backup_ems_pos
	stosw	; top_offset
	mov	AX,word ptr super_backup_ems_pos+2
	stosw
	mov	AX,[BP+first_pat]
	stosw	; top_number
	mov	AX,CX				; �p�^�[����
	stosw	; num_data
	mov	SI,[BP+first_pat]		; �p�^�[���T�C�Y���X�g�̓]��
	shl	SI,1
	add	SI,offset super_patsize
	rep	movsw

	; ���v�T�C�Y���v�Z����
	xor	SI,SI		; SIDI�Ƀo�C�g��������
	mov	DI,SI

	mov	CX,[BP+first_pat]
COUNT_LOOP:
	mov	BX,CX
	shl	BX,1
	mov	AX,super_patsize[BX]
	mul	AH
	add	DI,AX
	shl	AX,2
	add	DI,AX
	adc	SI,0
	inc	CX
	cmp	CX,[BP+last_pat]
	jle	short COUNT_LOOP

	; SIDI = �K�v�Ƃ��钷��
	;
	mov	AX,SI
	or	AX,DI
	jz	BACKUP_FAILURE

	cmp	super_backup_ems_handle,0
	jne	short BACKUP_EXPAND
	push	SI				; ���������߂͊m��
	push	DI
	_call	EMS_ALLOCATE
	test	AX,AX
	jz	short BACKUP_NOMEM
	mov	super_backup_ems_handle,AX
	xor	AX,AX
	mov	word ptr super_backup_ems_pos,AX	; 0�Ԓn��
	mov	word ptr super_backup_ems_pos+2,AX
	jmp	short BACKUP_START
	EVEN
BACKUP_EXPAND:					; �ɂ����߂͊g��
	mov	AX,word ptr super_backup_ems_pos
	mov	DX,word ptr super_backup_ems_pos+2
	add	AX,DI
	adc	DX,SI
	push	super_backup_ems_handle
	push	DX
	push	AX
	_call	EMS_REALLOCATE
	test	AX,AX
	jnz	short BACKUP_NOMEM

	; �f�[�^���̂̓]���J�n
BACKUP_START:
	mov	SI,[BP+first_pat]
BACKUP_LOOP:
	mov	BX,SI
	shl	BX,1
	mov	AX,super_patsize[BX]
	mul	AH
	mov	CX,AX
	shl	AX,2
	jz	short BACKUP_SKIP	; �󂫃f�[�^�͏������Ȃ�
	add	AX,CX
	push	super_backup_ems_handle	; handle
	push	word ptr super_backup_ems_pos+2	; offset
	push	word ptr super_backup_ems_pos
	push	super_patdata[BX]	; mem
	push	0
	push	0			; len
	push	AX
	add	word ptr super_backup_ems_pos,AX
	adc	word ptr super_backup_ems_pos+2,0
	_call	EMS_WRITE
BACKUP_SKIP:
	inc	SI
	cmp	SI,[BP+last_pat]
	jle	short BACKUP_LOOP

	xor	AX,AX		; NoError, clc
BACKUP_END:
	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2

BACKUP_INVAL:
	mov	AX,InvalidData
	jmp	short BACKUP_ERROR
BACKUP_FAILURE:
	push	ES		; ES=�Ǘ��u���b�N
	call	HMEM_FREE
	mov	AX,GeneralFailure
	jmp	short BACKUP_ERROR
BACKUP_NOMEM:
	mov	AX,InsufficientMemory
BACKUP_ERROR:
	stc
	jmp	short BACKUP_END
endfunc		; }


func SUPER_RESTORE_EMS	; super_restore_ems() {
	enter	8,0
	push	SI
	push	DI

	;
	; ����
	handle = (RETSIZE+2)*2
	load_to = (RETSIZE+1)*2
	read_off = -4
	read_adr = -8

	mov	ES,[BP+handle]

	mov	SI,[BP+load_to]
	cmp	SI,-1
	jne	short RESTORE_1
	mov	SI,ES:[0].top_number
RESTORE_1:
	mov	DI,ES:[0].num_data

	mov	AX,SI
	add	AX,DI
	jc	short RESTORE_INVAL	; safety..
	cmp	AX,MAXPAT
	ja	short RESTORE_INVAL

	mov	AX,word ptr ES:[0].top_offset
	mov	[BP+read_off],AX
	mov	AX,word ptr ES:[0].top_offset+2
	mov	[BP+read_off+2],AX

	mov	word ptr [BP+read_adr],offset [0].patsize
	mov	AX,[BP+handle]
	mov	[BP+read_adr+2],AX

RESTORE_LOOP:
	les	BX,[BP+read_adr]
	mov	AX,ES:[BX]
	mov	BX,AX

	mul	AH
	mov	CX,AX
	shl	AX,2
	jz	short RESTORE_SKIP	; �󂫃f�[�^�͏������Ȃ�
	add	CX,AX
	push	CX
	mov	mem_AllocID,MEMID_super
	_call	HMEM_ALLOCBYTE
	jc	short RESTORE_NOMEM

	push	SI			; patnum for super_entry_at
	push	BX			; patsize for super_entry_at
	push	AX			; patseg for super_entry_at

	push	super_backup_ems_handle
	push	[BP+read_off+2]
	push	[BP+read_off]
	push	AX				; pagseg
	xor	AX,AX
	push	AX				; 0
	push	AX				; 0
	push	CX				; pattern bytes
	add	[BP+read_off],CX
	adc	[BP+read_off+2],AX	; 0
	_call	EMS_READ

	_call	SUPER_ENTRY_AT
RESTORE_SKIP:
	inc	SI
	add	word ptr [BP+read_adr],2
	dec	DI
	jnz	short RESTORE_LOOP

	xor	AX,AX
RESTORE_RET:
	pop	DI
	pop	SI
	leave
	ret	4

RESTORE_INVAL:
	mov	AX,InvalidData
	jmp	short RESTORE_FAILURE
RESTORE_NOMEM:
	mov	AX,InsufficientMemory
RESTORE_FAILURE:
	stc
	jmp	short RESTORE_RET
endfunc		; }


func SUPER_FREE_EMS	; super_free_ems() {
	xor	AX,AX
	xchg	AX,super_backup_ems_handle
	test	AX,AX
	jz	short FREE_RET
	push	AX
	_call	EMS_FREE
FREE_RET:
	ret
endfunc		; }

END
