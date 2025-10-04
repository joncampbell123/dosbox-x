; master library - PC-9801
;
; Description:
;	�^�C�}���荞��
;		�J�n - timer_start
;		�I�� - timer_end
;
; Function/Procedures:
;	int timer_start( unsigned count, void (far * pfunc)(void) ) ;
;	void timer_end(void) ;
;	void timer_leave(void) ;
;
; Parameters:
;	count	�^�C�}���荞�݂̎����B�P�ʂ̓^�C�}�ɂ���ĈقȂ�B
;		0�͋֎~�B
;	pfunc	�^�C�}���荞�ݏ������[�`���̃A�h���X�B
;		timer_Count���J�E���g���邾���ő��ɏ������s�v�Ȃ̂Ȃ�
;		0�����Ă��悢�B
;
; Returns:
;	timer_start:
;		0=���s(���łɓ����Ă���)
;		1=����
;
; Global Variables:
;	void (far * timer_Proc)(void) ;
;	unsigned long timer_Count ;
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
;	�@�^�C�}���荞�݃��[�`���̒��ł́A���W�X�^�̕ۑ������A����Ȃǂ�
;	�ʏ�̊֐��Ƃقړ��l�ł��B�X�^�b�N�Z�O�����g���s���Ȃ��ƂƁA
;	���荞�݂��֎~��Ԃł��邱�Ƃɒ��ӂ��Ă��������B
;	�@EOI�𔭍s����͕K�v����܂���B
;
;	�@timer_start�����s������ADOS�ɖ߂�܂łɕK��timer_end�����s����
;	�������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 3/19 Initial: timer.asm/master.lib 0.23
;	94/ 4/10 [M0.23] bugfix, timer_start�̈������Ⴄ�Ƃ��납�����Ă��Ă�
;	94/ 6/21 [M0.23] timer_start��timer_Count��0�N���A����悤�ɂ���

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

TIMER_VECT	EQU 8	; �^�C�}���荞�݂̃x�N�^
IMR		EQU 2	; ���荞�݃}�X�N���W�X�^
TIMER_DISABLE	EQU 1	; �^�C�}���荞�݂̃}�X�N�r�b�g


	.DATA
	EXTRN timer_Proc : DWORD	; tim.asm
	EXTRN timer_Count : DWORD	; tim.asm

timer_OldMask DB	0
	EVEN

	.DATA?
timer_OldVect DD	?

	.CODE

TIMER_CNT	equ	71h
TIMER_SET	equ	77h


; TIMER���荞�݂̏����ݒ�ƊJ�n
func TIMER_START	; timer_start() {
	push	BP
	mov	BP,SP
	; ����
	count	= (RETSIZE+3)*2
	pfunc	= (RETSIZE+1)*2

	xor	AX,AX
	mov	word ptr timer_Count,AX
	mov	word ptr timer_Count+2,AX

	cmp	timer_OldMask,AL ; house keeping
	jne	short S_IGNORE

	mov	AX,[BP+pfunc]
	mov	word ptr timer_Proc,AX
	mov	AX,[BP+pfunc+2]
	mov	word ptr timer_Proc+2,AX

	mov	AL,TIMER_VECT	; TIMER���荞�݃x�N�^�̐ݒ�ƕۑ�
	push	AX
	push	CS
	mov	AX,offset TIMER_ENTRY
	push	AX
	call	DOS_SETVECT
	mov	word ptr timer_OldVect,AX
	mov	word ptr timer_OldVect+2,DX

	pushf
	CLI

	mov	AL,36h		; ���[�h(timer#0 MODE#3 LSB+HSB BINARY)
	out	TIMER_SET,AL
	mov	AX,[BP+count]	; ����
	out	TIMER_CNT,AL
	mov	AL,AH
	out	TIMER_CNT,AL

				; �ȑO��TIMER���荞�݃}�X�N�̎擾��
	in	AL,IMR		; TIMER���荞�݂̋���
	mov	AH,AL
	and	AL,NOT TIMER_DISABLE
	out	IMR,AL

	popf

	or	AH,NOT TIMER_DISABLE
	mov	timer_OldMask,AH

	mov	AX,1	; success

S_IGNORE:
	pop	BP
	ret	6
	EVEN
endfunc			; }

; INT 08h TIMER���荞��
TIMER_ENTRY proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX
	add	word ptr timer_Count,1
	adc	word ptr timer_Count+2,0

	cmp	WORD PTR timer_Proc+2,0
	je	short TIMER_COUNT_END
	push	BX
	push	CX
	push	DX
	push	SI	; for pascal
	push	DI	; for pascal
	push	ES
	CLD
	call	DWORD PTR timer_Proc
	pop	ES
	pop	DI	; for pascal
	pop	SI	; for pascal
	pop	DX
	pop	CX
	pop	BX
	CLI

TIMER_COUNT_END:
	pop	DS
	mov	AL,20h		; EOI
	out	0,AL		; send EOI to master PIC
	pop	AX
	iret
	EVEN
TIMER_ENTRY endp


; TIMER���荞�݂̏I���ƕ���
	public TIMER_LEAVE
func TIMER_END		; timer_end() {
TIMER_LEAVE label callmodel
	cmp	timer_OldMask,0 ; house keeping
	je	short E_IGNORE

	pushf
	CLI

	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	TIMER_SET,AL
	mov	AX,19968	; ������10ms�ɂ���
	jnz	short SKIP1
	mov	AX,24576
SKIP1:
	out	TIMER_CNT,AL
	jmp	$+2
	mov	AL,AH
	out	TIMER_CNT,AL

	mov	AX,TIMER_VECT	; TIMER���荞�݃x�N�^�̕���
	push	AX
	push	word ptr timer_OldVect + 2
	push	word ptr timer_OldVect
	call	DOS_SETVECT

	in	AL,IMR		; TIMER���荞�݃}�X�N�̕���
	and	AL,timer_OldMask
	out	IMR,AL

	popf

	xor	AL,AL
	mov	timer_OldMask,AL

E_IGNORE:
	ret
endfunc			; }

END
