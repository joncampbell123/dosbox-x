; master library - BGM
;
; Description:
;	�Ȃ�I������
;
; Function/Procedures:
;	int bgm_select_music(int num);
;
; Parameters:
;	num			�Ȕԍ�
;
; Returns:
;	BGM_COMPLETE		����I��
;	BGM_NO_MUSIC		�����ȋȔԍ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
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
;	femy(��  ����)		: �I���W�i���EC�����
;	steelman(���  �T�i)	: �A�Z���u�������
;
; Revision History:
;	93/12/19 Initial: b_s_msic.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	part:word	;SPART

	.CODE
	EXTRN	_BGM_BELL_ORG:CALLMODEL
	EXTRN	_BGM_MGET:CALLMODEL
	EXTRN	_BGM_PINIT:CALLMODEL
	EXTRN	BGM_SET_TEMPO:CALLMODEL

func BGM_SELECT_MUSIC
	mov	BX,SP
	push	SI
	push	DI
	num	= (RETSIZE+0)*2

	;�Ȕԍ��`�F�b�N
	mov	SI,SS:[BX+num]
	cmp	SI,1
	jl	short ILLEGAL
	cmp	SI,glb.mnum
	jle	short OK
ILLEGAL:
	mov	AX,BGM_NO_MUSIC
	pop	DI
	pop	SI
	ret	2
OK:
	call	_BGM_BELL_ORG
	mov	glb.rflg,OFF
	mov	glb.mcnt,SI
	;bgm_set_tempo(glb.mtp[glb.mcnt -1]);
	shl	SI,1
	push	glb.mtp[SI]
	call	BGM_SET_TEMPO

	;�p�[�g������
	xor	DI,DI		;pcount
	mov	SI,offset part
even
PINITLOOP:
	;bgm_pinit(part + pcnt);
	push	SI
	call	_BGM_PINIT
	;part[pcount].mask = ((glb.mask & (1 << pcount)) ? ON : OFF);
	mov	AX,1
	mov	CX,DI
	shl	AX,CL
	test	AX,glb.pmask
	je	short MASKISOFF
	mov	AX,ON
	jmp	short MASKISON
MASKISOFF:
	xor	AX,AX		;OFF
MASKISON:
	mov	[SI].msk,AX
	;bgm_mget(part + pcnt);
	push	SI
	call	_BGM_MGET
	add	SI,type SPART
	inc	DI
	cmp	DI,PMAX
	jne	short PINITLOOP

	xor	AX,AX			; BGM_COMPLETE
	pop	DI
	pop	SI
	ret	2
endfunc
END
