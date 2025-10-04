; ��Ԃ̏���Ԃ𑪒肷��[PC/AT]
;
; Function/Procedures:
;	void perform_at_start(void) ;
;	unsigned long perform_at_stop(void) ;
;
; Usage:
;   perform_start()�����s�����u�Ԃ���Aperform_stop()�����s����܂ł�
;   ���Ԃ𑪒肵�Aperform_stop()�̖߂�l�Ƃ��ĕԂ��B
;   �@�������A���̒l�͓����I�Ȃ��̂Ȃ̂ŁAperform_str()���g���Ď����������
;   �ϊ����ė��p���邱�ƁB
; 
; �Ώۋ@��:
; 	PC/AT
; 
; �v�����x:
; 	�v���P��: 1/1193.18msec
; 	�Œ��͈�: 174760*2(215089.2308*2)msec  <-mada keisan sitenai
; 		���b���Z�Ŗ�349(430)�b
;
; 	�v���덷: ���s���ɂ����銄�荞�݂ɉe�����A40msec����0.5msec���x�c�B
; 
; Notes:
; 	�^�C�}���荞�݂��g�p����v���O�����Ƃ͋����ł��܂���B
; 	�i���Ɏg���Ă���^�C�}���荞�݂͒�~���Ă��܂��܂��j
;
; �����ɂ���:
;	�^�C�}���荞�݂̃��[�h�������V���b�g�ɂ��Ă��闝�R�ł����A
;	����͎��ۂɃA�v���P�[�V��������������Ԃ��v�邽�߂�
;	�v���̃I�[�o�[�w�b�h���v���͈͂���ǂ��o�����߂ɍs�������u�ł��B
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
; 	���ˏ��F
; 
; Revision History:
; 	92/5/25 Initial ( ���߲׈ˑ������炷���߁Aperform.c��蕪�� )
;	92/6/17 TASM, Pacsal�Ή�
;	92/11/21 master.lib�ɒǉ�
;	92/12/20 perform_stop()�Ń^�C�}#0��mode 3,10ms�ɐݒ肷��悤�ɂ���
;	93/ 8/ 4 [M0.20] �x�N�^���A���o���ĂȂ������̂� bugfix(^^;
;	93/12/ 7 Initial: perfat.asm/master.lib 0.22
;

	.MODEL SMALL

	.DATA?

	EXTRN perform_Rate:WORD
LASTTINT	dd	?
TIMER		dw	?

	.CODE
	include func.inc

; �^�C�}���荞�݂̃G���g��
timer_entry	PROC	FAR
	push	AX
	push	DS
	mov	AX,seg TIMER
	mov	DS,AX
	inc	TIMER
	pushf
	call	LASTTINT	; includes EOI
	pop	DS
	mov	AL,30h
	out	43h,AL		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	mov	AL,0FFh
	out	40h,AL		; counter LSB
	out	40h,AL		; counter MSB, counter start
	pop	AX
	iret
	EVEN
timer_entry	ENDP

func PERFORM_AT_START	; perform_at_start() {
	xor	AX,AX
	mov	TIMER,AX
	CLI
	mov	DX,21h
	in	AL,DX
	or	AL,1		; MASK!
	out	DX,AL
	mov	AL,0
	STI

	mov	AX,3508h	; ���荞�݃x�N�^08h�̓ǂݎ��
	int	21h
	mov	word ptr LASTTINT,BX
	mov	word ptr LASTTINT+2,ES

	push	DS
	mov	DX,offset timer_entry
	mov	AX,seg timer_entry
	mov	DS,AX
	mov	AX,2508h	; ���荞�݃x�N�^08h�̏�������
	int	21h
	pop	DS

	CLI

	mov	AL,30h		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	out	43h,AL

	mov	AL,0FFh
	out	40h,AL		; counter LSB
	out	40h,AL		; counter MSB, counter start

	mov	DX,21h
	in	AL,DX
	and	AL,0FEh		; Clear Mask
	out	DX,AL

	STI
	ret
	EVEN
endfunc			; }


;
; unsigned long _far perform_stop(void) ;
; function perform_stop : Longint ;
FUNC PERFORM_AT_STOP	; perform_at_stop() {
	CLI
	mov	AL,0	; latch
	out	043h,AL

	mov	DX,40h
	in	AL,DX
	mov	AH,AL
	in	AL,DX
	xchg	AH,AL	; w = inp(0x40) + inp(0x40) << 8 ;
	mov	DX,TIMER ; l = (TIMER * 0x10000L) + (0xffffL - w) ;
	not	AX

	mov	perform_Rate,11932	; 1193.18K

	push	AX	; push l
	push	DX

	mov	DX,21h	; MASK
	in	AL,DX
	or	AL,1
	out	DX,AL

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	43h,AL
	mov	AL,0ffh
	out	40h,AL
	jmp	$+2
	out	40h,AL

	STI

	push	DS
	lds	DX,LASTTINT
	mov	AX,2508h	; ���荞�݃x�N�^08h�̏�������
	int	21h
	pop	DS

	CLI
	mov	DX,21h
	in	AL,DX
	and	AL,0FEh		; Clear Mask
	out	DX,AL
	STI

	pop	DX	; pop l
	pop	AX

	ret
endfunc			; }

END
