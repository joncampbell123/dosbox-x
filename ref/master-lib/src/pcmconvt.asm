; master library - PC98V - PCM
;
; Description:
;	pcm_play�p��PCM�f�[�^�̏���
;
; Function:
;	void pcm_convert( void far * dest , const void far * src,unsigned rate,unsigned long size)
;
; Parameters:
;	void far *dest	= �i�[�ꏊ�̐擪�A�h���X
;	const void far *src	= PCM�f�[�^�̐擪�A�h���X
;	unsigned rate	= ���t���[�g (1�p���X�̑傫��)(1~65535)
;	unsigned long size	= �p���X��(0�Ȃ牉�t���ɂႢ)

; Returns:
;	none

; Binding Target:
;	MSC/TC/BC/TP

; Running Target:
;	MS-DOS

; Requiring Resources:
;	CPU: 8086

; Compiler/Assembler:
;	OPTASM 1.6
;	TASM
;
; Notes:
;	src�f�[�^�`���́A�x�^��8bit PCM�f�[�^�ł��B
;	dest�ɂ́Asize�o�C�g�̃��������K�v�ł��B
;	src��dest�͈�v�ł��܂��B
;
; Author:
;	�V��r�� (SuCa: pcs28991 fem20932)
;
; Rivision History:
;	92/10/26 Initial
;	92/12/03 ����
;	92/12/09 8bit�Œ�, 1�̕␔��(koizuka)
;	92/12/14 1�̕␔���ĈӖ��Ȃ�������
;	95/ 3/15 [M0.22k] BUGFIX: src,dest�̏������t������

	.MODEL SMALL
	include func.inc

	.CODE

func PCM_CONVERT	; pcm_convert() {
	push	BP
	mov	BP,SP

	; ����
	dst_seg	= (RETSIZE+7)*2
	dst_off	= (RETSIZE+6)*2
	src_seg	= (RETSIZE+5)*2
	src_off	= (RETSIZE+4)*2
	rate	= (RETSIZE+3)*2
	pcm_lh	= (RETSIZE+2)*2
	pcm_ll	= (RETSIZE+1)*2

	push	DS
	push	SI
	push	DI

	lds	SI,[BP+src_off]	; DS:SI = ���f�[�^
	xor	AX,AX
	les	DI,[BP+dst_off]	; ES:DI = ��f�[�^

	mov	CX,[BP+pcm_ll]	; BPCX = �f�[�^��
	mov	BX,[BP+rate]	; BX = ���t���[�g
	mov	BP,[BP+pcm_lh]

	inc	BP
	jcxz	short SEGCHK

	EVEN
OKAY1:
	mov	AL,[SI]		; 1�o�C�g���
				; scale max from 255 to BL
	mul	BL		; AH = AL * BL / 256
	mov	ES:[DI],AH	; ������������
	inc	DI
	jnz	short SEND

	mov	AX,ES
	add	AX,1000h
	mov	ES,AX

SEND:
	inc	SI
	jnz	short OKAY2

	mov	AX,DS
	add	AX,1000h
	mov	DS,AX

	; BPCX�̉񐔂̃��[�v
OKAY2:
	loop	short OKAY1
SEGCHK:
	dec	BP
	jnz	short OKAY1

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	14
endfunc			; }

END
