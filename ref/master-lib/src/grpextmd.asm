; master library - 9821
;
; Description:
;	9821�g���O���t�B�b�N���[�h�̐ݒ�^�擾
;
; Function/Procedures:
;	unsigned graph_extmode( unsigned modmask, unsigned bhal ) ;
;
; Parameters:
;	modmask:���8bit: BH�𑀍삷��r�b�g�}�X�N
;	modmask:����8bit: AL�𑀍삷��r�b�g�}�X�N
;	bhal	���8bit: BH�ɐݒ肷��l(modmask�ɑΉ������r�b�g�̂ݗL��)
;		����8bit: AL�ɐݒ肷��l(modmask�ɑΉ������r�b�g�̂ݗL��)
;
;		�r�b�g�̓��e:
;			AL  b7 b6 b5 b4 b3 b2 b1 b0
;						 ++- 0 �m���C���^�[���[�X
;						     1 �C���^�[���[�X
;					++-++------- 00 15.98kHz
;						     10 24.83kHz
;						     11 31.47kHz
;			BH  b7 b6 b5 b4 b3 b2 b1 b0
;				  || ||	      ++-++- 00 20�s
;				  || ||		     01 25�s
;				  || ||		     10 30�s
;				  ++-++------------- 00 640x200(UPPER)
;						     01 640x200(LOWER)
;						     10 640x400
;						     11 640x480
;	
;
; Returns:
;	���ۂɐݒ肵���l(almod=bhmod=0�Ȃ�Ύ擾�����l)
;	(���8bit=BH, ����8bit=AL��)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801/9821, ������9821�łȂ��Ǝ��s���Ă��������Ȃ��� 0 ��Ԃ��B
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
;	���ˏ��F
;
; Revision History:
;	94/ 1/ 8 Initial: grpextmd.asm/master.lib 0.22

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextShown:WORD

	.CODE

func GRAPH_EXTMODE	; graph_extmode() {
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[045ch],40h
	jz	short G31_NOTMATE
	;
	modmask = (RETSIZE+1)*2
	bhal = (RETSIZE+0)*2
	mov	BX,SP
	mov	CX,SS:[BX+modmask]
	mov	DX,SS:[BX+bhal]
	mov	AH,31h
	int	18h		; �g���O���t�A�[�L�e�N�`�����[�h�̎擾
	mov	AH,BH
	jcxz	short EXT_DONE	; modmask=0��������ǂݎ���ďI���
	and	DX,CX
	not	CX
	and	AX,CX
	or	AX,DX
	mov	CX,AX
	mov	BH,AH

	mov	AH,30h		; �g���O���t�A�[�L�e�N�`�����[�h�̐ݒ�
	int	18h

	test	TextShown,1
	jz	short TEXT_SHOWN
	mov	AH,0ch
	int	18h		; �e�L�X�g��ʂ̕\��
TEXT_SHOWN:
	test	CL,1
	jz	short NO_SETAREA
	mov	AH,0eh		; ��̕\���̈�̐ݒ�
	xor	DX,DX
	int	18h
NO_SETAREA:
	test	byte ptr ES:[0711h],1
	jz	short CURSOR_HIDDEN
	mov	AH,11h		; �J�[�\���̕\��
	int	18h
CURSOR_HIDDEN:
	mov	AX,CX
EXT_DONE:
G31_NOTMATE:
	ret	4
endfunc		; }

END
