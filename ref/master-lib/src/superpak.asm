; master library - superimpose packedpixel - entry
;
; Description:
;	�p�b�N16�F�f�[�^����p�^�[����o�^����
;
; Functions/Procedures:
;	int super_entry_pack( const void far * image, unsigned image_width, int patsize, int clear_color ) ;
;
; Parameters:
;	image		packed data�̓ǂݎ��J�n�A�h���X
;	image_width	packed data�S�̂̉��h�b�g��(2dot�P��, �[���͐؂�グ)
;	patsize		�o�^����p�^�[���̑傫��
;	clear_color	�����F
;
; Returns:
;	0�`511		     �o�^���ꂽ�p�^�[���ԍ�
;	InsufficientMemory   ������������Ȃ�
;	GeneralFailure	     �p�^�[���̓o�^������������
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	93/11/22 Initial: superpak.asm/master.lib 0.21
;	95/ 4/ 3 [M0.22k] �������̌���������������������

	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	SUPER_ENTRY_PAT:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
	endm

retfunc FAILURE
	mov	AX,GeneralFailure
NOMEM:
	STC
	MRETURN
endfunc

func SUPER_ENTRY_PACK	; super_entry_pack() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	image	    = (RETSIZE+4)*2
	image_width = (RETSIZE+3)*2
	patsize	    = (RETSIZE+2)*2
	clear_color = (RETSIZE+1)*2

	mov	AX,[BP+patsize]		; SI = patsize( hi:Xdots, lo:Ydots )
	mov	SI,AX
	mov	CL,AH
	mov	CS:xbytes,CL
	mov	CH,0			; CX = Xdots
	mov	AH,0
	mul	CX			; AX = size of a plane
	test	DX,DX
	jnz	short FAILURE
	mov	DX,AX			; DX = plane size
	shl	AX,1			; 4plane��
	jc	short FAILURE
	shl	AX,1
	jc	short FAILURE
	mov	BX,AX			; BX = 4plane���̃o�C�g��
	push	AX
	call	SMEM_WGET
	jc	short NOMEM

	mov	ES,AX			; ES = segment of temporary buffer
	xor	BX,BX

	shl	CX,1
	shl	CX,1			; CX = Xdots*4(�o�C�g��)
	mov	DI,[BP+image_width]
	shr	DI,1
	adc	DI,BX			; BX=0
	sub	DI,CX			; DI = image_width/2 - ���o�C�g��
					; ����ɂ��Aimage��1line�̍����o��

	mov	CX,SI			; CX = patsize

	; super_entry_pat�̈�������push
	push	CX			; patsize _patsize
	push	ES			; seg addr
	push	BX			; offset address = 0
	push	[BP+clear_color]	; clear_color�͈������̂܂�

	push	DS			; DS ��Ҕ�
	lds	SI,[BP+image]

	mov	BP,DI			; BP�ɁAimage��1line�̍����ʂ�

next_y:
	mov	CH,0	; dummy
	org $-1
xbytes	db	?

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
	dec	CH
	jnz	short next_x

	add	SI,BP			; image������line�ɐi�߂�
	; CH=0����
	loop	short next_y

	pop	DS			; DS�𕜌�

	mov	SI,ES			; SI�ɊJ������segment��Ҕ�

	; �����͂��ł�push���Ă��邩�炻�̂܂�call
	call	SUPER_ENTRY_PAT		; (patsize, far addr, clear_color)
	push	SI
	call	SMEM_RELEASE		; ���̊֐���AX��flag���ۑ�����Ă���
	MRETURN
endfunc		; }

END
