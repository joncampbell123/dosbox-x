; master library - PC-9801
;
; Description:
;	VSYNC���荞��
;		�J�n - vsync_start
;		�I�� - vsync_end
;
; Function/Procedures:
;	void vsync_start(void) ;
;	void vsync_end(void) ;
;	void vsync_leave(void) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Global Variables:
;	unsigned volatile vsync_Count1, vsync_Count2 ;
;		VSYNC���荞�ݖ��ɑ�����������J�E���^�B
;		vsync_start�� 0 �ɐݒ肳��܂��B
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
;	�Evsync_start���s������A�K��vsync_end�����s���Ă��������B
;	�@�����ӂ�ƃv���O�����I����n���O�A�b�v���܂��B
;	�Evsync_start���Q�x�ȏ���s����ƁA�Q�x�ڈȍ~�̓J�E���^��
;	�@���Z�b�g���邾���ɂȂ�܂��B
;	�Evsync_end���Avsync_start���s�킸�Ɏ��s���Ă��������܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/21 Initial
;	93/ 2/10 vsync_Proc�p�����ǉ�
;	93/ 4/19 ���荞�݃��[�`����CLD��Y��Ă���
;	93/ 6/24 [M0.19] CLI-STI�� pushf-CLI-popf�ɕύX
;	93/ 8/ 8 [M0.20] vsync_wait�� vsyncwai.asm�ɕ���
;	95/ 1/30 [M0.23] vsync_Delay�ɂ��x���ǉ�
;	95/ 1/31 [M0.23] vsync_start() 31kHz���ۂ���vsync_Delay�ɒl��ݒ肷��

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL


CRTBIOS_VECT	EQU 18h
VSYNC_VECT	EQU 0ah
IMR		EQU 2	; ���荞�݃}�X�N���W�X�^
VSYNC_DISABLE	EQU 4


	.DATA
	EXTRN vsync_Count1 : WORD	; ������������J�E���^1
	EXTRN vsync_Count2 : WORD	; ������������J�E���^2
	EXTRN vsync_Proc : DWORD
	EXTRN vsync_Delay : WORD

	EXTRN vsync_OldVect : DWORD	; int 0ah(vsync)
	EXTRN vsync_OldMask : BYTE

	.DATA?
vsync_delay_count dw ?

	.CODE
	EXTRN GRAPH_EXTMODE:CALLMODEL

raw_crtbios	dw	?	; int 18h ( CRT BIOS )
raw_crtbios_seg	dw	?

; VSYNC���荞�݂̏����ݒ�ƊJ�n
;	void vsync_start( void ) ;
func VSYNC_START
	xor	AX,AX
	push	AX
	push	AX
	call	GRAPH_EXTMODE
	and	AX,0ch
	cmp	AX,0ch
	mov	vsync_Delay,13311
	je	short NOW_31KHz
	mov	vsync_Delay,0		; 24kHz
NOW_31KHz:

	xor	AX,AX
	mov	vsync_Count1,AX
	mov	vsync_Count2,AX

	cmp	vsync_OldMask,AL ; house keeping
	jne	short S_IGNORE

	mov	AL,VSYNC_VECT	; VSYNC���荞�݃x�N�^�̐ݒ�ƕۑ�
	push	AX
	push	CS
	mov	AX,offset VSYNC_COUNT
	push	AX
	call	DOS_SETVECT
	mov	word ptr vsync_OldVect,AX
	mov	word ptr vsync_OldVect + 2,DX

	pushf
	CLI			; �ȑO��VSYNC���荞�݃}�X�N�̎擾��
	in	AL,IMR		; VSYNC���荞�݂̋���
	mov	AH,AL
	and	AL,NOT VSYNC_DISABLE
	out	IMR,AL
	popf
	or	AH,NOT VSYNC_DISABLE
	mov	vsync_OldMask,AH

	mov	AX,CRTBIOS_VECT	; CRT BIOS���荞�݃x�N�^�̐ݒ�ƕۑ�
	push	AX
	push	CS
	mov	AX,offset CRTBIOS_COOK
	push	AX
	call	DOS_SETVECT
	mov	CS:raw_crtbios, AX
	mov	CS:raw_crtbios_seg, DX

	out	64h,AL		; VSYNC���荞�݂̋N������
S_IGNORE:
	ret
	EVEN
endfunc

; INT 18h ��VSYNC���荞�ݍċN��������
CRTBIOS_COOK proc
	pushf
	call	DWORD PTR CS:raw_crtbios
	out	64h,AL		; VSYNC���荞�݂̋N������
	iret
	EVEN
CRTBIOS_COOK endp


; INT 0ah VSYNC���荞��
VSYNC_COUNT proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX

	mov	AX,vsync_Delay
	add	vsync_delay_count,AX
	jc	short VSYNC_COUNT_END

	inc	vsync_Count1
	inc	vsync_Count2

	cmp	WORD PTR vsync_Proc+2,0
	je	short VSYNC_COUNT_END
	push	BX
	push	CX
	push	DX
	push	SI	; for pascal
	push	DI	; for pascal
	push	ES
	CLD
	call	DWORD PTR vsync_Proc
	pop	ES
	pop	DI	; for pascal
	pop	SI	; for pascal
	pop	DX
	pop	CX
	pop	BX
	CLI

VSYNC_COUNT_END:
	pop	DS
	mov	AL,20h		; EOI
	out	0,AL		; send EOI to master PIC
	out	64h,AL		; VSYNC���荞�݂̋N������
	pop	AX
	iret
	EVEN
VSYNC_COUNT endp


; VSYNC���荞�݂̏I���ƕ���
;	void vsync_end( void ) ;
;	void vsync_leave( void );
	public VSYNC_LEAVE
func VSYNC_END
VSYNC_LEAVE label callmodel
	cmp	vsync_OldMask,0 ; house keeping
	je	short E_IGNORE

	mov	AX,CRTBIOS_VECT	; CRT BIOS���荞�݃x�N�^�̕���
	push	AX
	push	CS:raw_crtbios_seg
	push	CS:raw_crtbios
	call	DOS_SETVECT

	pushf
	CLI
	in	AL,IMR		; VSYNC���荞�݂��֎~����
	or	AL,4
	out	IMR,AL
	popf

	mov	AX,VSYNC_VECT	; VSYNC���荞�݃x�N�^�̕���
	push	AX
	push	word ptr vsync_OldVect + 2
	push	word ptr vsync_OldVect
	call	DOS_SETVECT

	pushf
	CLI
	in	AL,IMR		; VSYNC���荞�݃}�X�N�̕���
	and	AL,vsync_OldMask
	out	IMR,AL
	popf
	out	64h,AL		; VSYNC���荞�݂̋N������

	xor	AL,AL
	mov	vsync_OldMask,AL

E_IGNORE:
	ret
	EVEN
endfunc

END
