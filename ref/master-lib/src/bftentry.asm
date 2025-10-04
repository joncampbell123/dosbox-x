; superimpose & master library module
;
; Description:
;	BFNT+�t�@�C��(16�F)����p�^�[����o�^����
;
; Functions/Procedures:
;	int bfnt_entry_pat( unsigned handle, BFNT_HEADER * header, int clear_color ) ;
;
; Parameters:
;	handle		BFNT+�t�@�C���n���h��
;	header		BFNT+�w�b�_
;	clear_color	�����F
;
; Returns:
;	NoError		    ����
;	InvalidData	    �t�@�C���n���h���܂��̓t�@�C�����ُ�
;	InsufficientMemory  ������������Ȃ�
;	GeneralFailure	    �o�^�p�^�[������������
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	
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
;$Id: bftentry.asm 0.05 93/01/15 11:37:08 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	93/ 3/16 ����
;	93/ 5/ 9 far bug fix
;	93/ 5/26 [M0.18] bug fix(32x32dot�ȏ�̃f�[�^���������ꂽ(^^;)
;	93/11/21 [M0.21] �ق�̂�����Ə���������, �t�@�C�����r���Ő؂�Ă�
;			�Ƃ��ɂ��G���[�ɂ���悤�ɂ���
;	95/ 3/24 [M0.22k] InvalidData���Asmem���J�����Ă��Ȃ�����

	.186
	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	SUPER_ENTRY_PAT:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

	.CODE

MRETURN macro
	pop	SI
	pop	DI
	pop	DS
	pop	BP
	ret	(2+DATASIZE)*2
	EVEN
	endm

retfunc INVAL
	push	DS
	mov	DS,CS:_DS_
	_call	SMEM_RELEASE
	mov	AX,InvalidData
FAULT:	MRETURN
endfunc

func BFNT_ENTRY_PAT	; bfnt_entry_pat() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; ����
	handle	= (RETSIZE+2+DATASIZE)*2
	header	= (RETSIZE+2)*2
	clear_color = (RETSIZE+1)*2

	mov	AX,[BP+clear_color]
	mov	CS:color,AX
	mov	CS:_DS_,DS
	mov	AX,[BP+handle]
	mov	CS:file_handle,AX
	_push	DS
	_lds	BX,[BP+header]
	mov	AX,(bfnt_header PTR [BX]).Xdots
	mov	CX,(bfnt_header PTR [BX]).Ydots
	mov	BP,(bfnt_header PTR [BX]).END_
	sub	BP,(bfnt_header PTR [BX]).START
	inc	BP			; BP = pattern counter
	_pop	DS
	shr	AX,3
	mov	BYTE PTR CS:[xsize],AL
	mov	DH,AL
	mov	DL,CL
	mov	CS:patsize,DX
	mov	BYTE PTR CS:[ysize],CL
	mul	CX
	mov	CS:plane_size,AX
	shl	AX,2			;pattern size (BRGI)
	mov	CS:read_byte,AX
	xchg	BX,AX
	push	BX
	_call	SMEM_WGET
	jc	short FAULT

	xchg	CX,AX			;address backup
	push	BX
	_call	SMEM_WGET
	jc	short FAULT

	mov	ES,AX
	mov	DS,CX

NEXT_PATTERN:
	JMOV	CX,read_byte
	xor	DX,DX
	JMOV	BX,file_handle

	mov	AH,3fh
	int	21h			;read handle
	jc	INVAL
	cmp	AX,CX
	jne	INVAL

	xor	SI,SI
	JMOV	DX,plane_size

	xor	BX,BX

	mov	CH,11h			;dummy
ysize		EQU	$-1

next_y:
	mov	CL,11h			;dummy
xsize		EQU	$-1

next_x:
	push	CX
	push	DX
	push	BX
	lodsw
	mov	BX,AX
	lodsw
	mov	DX,AX			; BL BH DL DH
	mov	DI,4
B2V_LOOP:
	rol	BL,1
	RCL	CH,1
	rol	BL,1
	RCL	CL,1
	rol	BL,1
	RCL	AH,1
	rol	BL,1
	RCL	AL,1

	rol	BL,1
	RCL	CH,1
	rol	BL,1
	RCL	CL,1
	rol	BL,1
	RCL	AH,1
	rol	BL,1
	RCL	AL,1

	mov	BL,BH
	mov	BH,DL
	mov	DL,DH

	dec	DI
	jnz	short B2V_LOOP

	pop	BX
	pop	DX

	; DI=0����
	mov	ES:[BX+DI],AL
	add	DI,DX
	mov	ES:[BX+DI],AH
	add	DI,DX
	mov	ES:[BX+DI],CL
	add	DI,DX
	mov	ES:[BX+DI],CH
	pop	CX

	inc	BX
	dec	CL
	jnz	short next_x

	dec	CH
	jnz	short next_y

	push	DS			;DS = data segment
	JMOV	AX,_DS_

	mov	DS,AX

	JMOV	AX,patsize
	push	ES

	push	AX			; patsize
	push	ES			; seg addr
	xor	AX,AX
	push	AX			;offset address
	JMOV	AX,color		;clear_color
	push	AX
	_call	SUPER_ENTRY_PAT		; (patsize, far addr, clear_color)

	pop	ES			;resister load !!

	pop	DS
	jc	short ERROR
	dec	BP
	jz	short RETURN
	jmp	NEXT_PATTERN
	EVEN

ERROR:
	push	DS
	mov	DS,CS:_DS_
	mov	CX,AX
	push	ES
	_call	SMEM_RELEASE
	_call	SMEM_RELEASE
	mov	AX,CX
	stc
	MRETURN
RETURN:
	push	DS
	mov	DS,CS:_DS_
	push	ES
	_call	SMEM_RELEASE
	_call	SMEM_RELEASE
	mov	AX,NoError
	clc
	MRETURN
endfunc		; }

END
