; master library - PC98V - PCM
;
; Description:
;	BEEP�����ɂ�� 8bitPCM�Đ�
;
; Function:
;	void pcm_play(const void far *pcm,unsigned rate,unsigned long size);
;
; Parameters:
;	const void far * pcm	= PCM�f�[�^�̐擪�A�h���X
;	unsigned rate		= ���t���[�g(�^�C�}���g��/�Đ����g��)(1~255)
;				  22KHz���K���l�ł��B
;	unsigned long size	= �p���X��(0�Ȃ牉�t���ɂႢ)
;
; Returns:
;	none
;
; Binding Target:
;	MSC/TC/BC/TP
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: 8086
;
; Compiler/Assembler:
;	OPTASM 1.6
;	TASM 2.0
;
; Notes:
;	pcm�f�[�^�́Apcm_convert�̕ϊ����ʂłȂ���΂Ȃ�܂���B
;	22KHz�Ȃ�Βʏ�̉�����Ƃ������x�̏o���ł��B
;
;	���t���͊��荞�݂��֎~���Ă��܂��B(�����ԂȂ̂Œ���)
;	���t��ABEEP���g���͊���l(2KHz)�ɁA
;	�^�C�}���荞�݂̎���������l(10ms)�ɐݒ肵�܂��B
;
; Author:
;	�V��r�� (SuCa: pcs28991 fem20932)
;	���ˏ��F (����: net19368 fem20047)
;
; Rivision History:
;	92/10/13 Initial
;	92/10/18 Beep�����@�\��Bugfix
;		 ���łɃf�[�^���󂳂Ȃ������ɂ��B
;	92/10/24 16bitRate�ɕύX(test)�B
;		 �v�Z�𕪗�
;	92/10/26 Rate�������ʋ@�\����
;	92/12/09 ����: 8bit�Œ�, TIMER�g�p
;	92/12/10 �����Ȃ������(�m�C�Y���C�ɂȂ���)
;	93/ 1/13 ���荞�݋֎~���^�C�}(��vsync)�̊��荞�݋֎~�����ɂ���
;	93/ 2/ 4 �o�O���(���荞�ݕ�������(���Ƃ���)������)
;	95/ 1/16 [M0.23] CLI/STI��pushf+CLI/popf�ɕύX�B

	.MODEL SMALL
	include func.inc
	.DATA?
ACTIVE db ?

	.CODE

IMR equ 2
TIMM equ 1
VSYM equ 4

func PCM_PLAY
	push	BP
	mov	BP,SP

	; ����
	pcm_seg	= (RETSIZE + 5)*2
	pcm_off	= (RETSIZE + 4)*2
	rate	= (RETSIZE + 3)*2
	pcm_lh	= (RETSIZE + 2)*2
	pcm_ll	= (RETSIZE + 1)*2

	push	DS
	push	SI
	push	DI

	; �^�C�}�EVSYNC���荞�݋֎~
	pushf
	CLI
	mov	DX,IMR
	in	AL,DX
	mov	AH,AL
	or	AH,NOT (TIMM OR VSYM)
	mov	ACTIVE,AH
	or	AL,TIMM or VSYM		; MASK!
	out	DX,AL
	mov	AL,0
	popf

	mov	AL,07h			; BEEP OFF
	out	37h,AL

	mov	AL,14h			; timer#0 MODE#2 LSB BINARY
	out	77h,AL

	mov	AL,50h			; timer#1 MODE#0 LSB BINARY
	mov	DX,3FDFh
	out	DX,AL

	lds	SI,[BP+pcm_off]		; DS:SI = �f�[�^�A�h���X
	mov	BX,DS
	mov	CX,[BP+pcm_ll]		; DICX = �f�[�^��
	mov	DI,[BP+pcm_lh]
	mov	AL,[BP+rate]
	out	71h,AL			; timer#0 ���t���[�g�w��

	jmp	short PLAYS
	EVEN
WAITHI:
	in	AL,71h
	cmp	AL,16
	jb	short WAITHI	; around��҂�

WAITREQ:
;	jmp	$+2
	in	AL,71h
	cmp	AL,16
	jnb	short WAITREQ	; around��҂�
;	mov	AL,07h		; BEEP OFF
;	out	37h,AL

	mov	AL,AH
	mov	DX,3FDBh	; �w�莞�Ԍ�� BEEP ON
	out	DX,AL

	mov	AL,06h		; BEEP ON
	out	37h,AL
	cmp	SI,1
	sbb	DX,DX
	and	DX,1000h
	add	BX,DX
	mov	DS,BX
PLAYS:
	lodsb			; 1�o�C�g���
	mov	AH,AL
	sub	CX,1
	sbb	DI,0
	jnb	short WAITHI

PLAY8E:				; counter hi-word decrement

	mov	AL,07h		; BEEP OFF
	out	37h,AL
	;----------
	; ���t�I��
	;----------

	; timer#0�� 10ms�̊���l�ɐݒ�

	mov	AL,76h		; timer����
	mov	DX,3FDFh
	out	DX,AL
	in	AL,42h
	and	AL,20h
	mov	AX,998
	mov	BX,19968
	jnz	short SKIP	; BEEP���̎��g����ʏ�ɖ߂�
	mov	AX,1229
	mov	BX,24576
EVEN
SKIP:
	mov	DX,3FDBh
	out	DX,AL
	mov	AL,AH
	out	DX,AL

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	77h,AL
	jmp	$+2
	jmp	$+2
	mov	AX,BX
	out	71h,AL
	jmp	$+2
	mov	AL,AH
	out	71h,AL

	pop	DI
	pop	SI
	pop	DS

	; �^�C�}�EVSYNC���荞�݂�����
	mov	AH,ACTIVE
	cmp	AH,0ffh
	je	short SKIP2
		pushf
		CLI
		mov	DX,2
		in	AL,DX
		and	AL,AH
		out	DX,AL
		out	64h,AL	; kick vsync
		popf
SKIP2:

	pop	BP
	ret	10
endfunc

END
