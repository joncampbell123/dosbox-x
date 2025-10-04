; master library - 
;
; Description:
;	�A�i���O�W���C�X�e�B�b�N�̓ǂݎ��
;
; Function/Procedures:
;	int js_analog( int player, unsigned char astat[4] ) ;
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 
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
;	93/ 6/20 Initial:jsanalog.asm/master.lib 0.19

	.MODEL SMALL
	include func.inc

	.DATA?

	EXTRN js_bexist:WORD
	EXTRN js_saj_port:WORD

	.CODE
	EXTRN SOUND_O : NEAR

READ_PULSE equ	2000

func JS_ANALOG	; {
	push	BP
	mov	BP,SP
	sub	SP,10
	push	DI

	; ����
	player	= (RETSIZE+1+DATASIZE)*2
	astat	= (RETSIZE+1)*2

	; ���[�J���ϐ�
	temp = -10

;	0	A B C D
;	1	E2 E1 START SELECT
;	2	ch0 high
;	3	ch1 high
;	4	ch2 high
;	5	ch3 high
;	6	ch0 low
;	7	ch1 low
;	8	ch2 low
;	9	ch3 low
;	10	A~ B~ A_ B_	; �ǂ�łȂ����� A~B~��stick, A_B_�͔�AB��
;	11			; �ǂ�łȂ�����

	pushf
	CLI

	mov	AX,[BP+player]	; 1=player1,  2=player2
	dec	AX
	cmp	AX,2
	jae	short HOP_ERROR

SAJ:
	mov	DX,js_saj_port
	test	DX,DX
	je	short JOYSTICK

	mov	CL,AL
	shl	AX,1	; BX = read port number
	mov	BX,AX
	add	BX,DX

	mov	AL,10h
	shl	AL,CL
	out	DX,AL
	mov	CX,1		; �p���X�̒���
SAJDELAY:
	loop	short SAJDELAY
	mov	AL,0
	out	DX,AL
	mov	DX,BX

	jmp	short SENSE_START
	EVEN
HOP_ERROR:
	jmp	ERROREXIT
	EVEN

JOYSTICK:
	mov	CX,220		; �p���X�̒���
	mov	AH,10

	cmp	js_bexist,0
	je	short ERROREXIT

	neg	AX
	sbb	BX,BX
	and	BX,40h
	or	BX,0fb0h
	call	SOUND_O		; ******** �܂��p���X�𑗂�̂�

JSDELAY:
	loop	short JSDELAY

	mov	AL,BL
	xor	AL,30H
	out	DX,AL

	mov	DX,188h
	mov	AL,0eh
	out	DX,AL		; ******** ���[�ăp���X�͑���������
	inc	DX		; �ǂݍ��݂��J�n���邩��
	inc	DX


SENSE_START:
	mov	AH,5		; 5 �o�C�g�J��Ԃ�( 10nibble )
	xor	DI,DI

SENSE_LOOP:
	mov	CX,READ_PULSE
WAIT1:
	in	AL,DX
	dec	CX
	jz	short ERROREXIT		; �^�C���A�E�g����

	test	AL,30h
	jnz	short WAIT1		;�ǂ����� 0�ɂȂ�܂ő҂�

		; �Ȃ����牺��4bit���i�[���ă|�C���^������
	and	AL,0fh
	mov	[BP+temp+DI],AL
	inc	DI

	mov	CX,READ_PULSE
WAIT2:
	mov	BH,0eh
	in	AL,DX
	dec	CX
	jz	short ERROREXIT

	test	AL,10h
	jz	short WAIT2		;1�ɂȂ�܂�
	test	AL,20h
	jnz	short WAIT2		;0�̂܂�

		; �Ȃ����牺��4bit���i�[���ă|�C���^������
	and	AL,0fh
	mov	[BP+temp+DI],AL
	inc	DI

	dec	AH
	jnz	short SENSE_LOOP

	popf		; ���荞�݋֎~������

	; �ǂݎ�肪�I������̂ŁA���ʂ��i�[����
	xor	AX,AX
	mov	CL,4
	_les	BX,[BP+astat]

	xor	DI,DI
STORE_LOOP:
	mov	AL,[BP+temp+2+DI]
	shl	AL,CL
	or	AL,[BP+temp+6+DI]
 l_	<mov	ES:[BX+DI],AL>
 s_	<mov	[BX+DI],AL>
	inc	DI
	cmp	DI,3
	jl	short STORE_LOOP

	mov	AX,[BP+temp]
	shl	AL,CL
	or	AL,AH
	not	AL
 l_	<mov	ES:[BX+DI],AL>
 s_	<mov	[BX+DI],AL>

	mov	AX,1
EXIT:
	pop	DI
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
	EVEN

ERROREXIT:
	popf
;	xor	AX,AX
	jmp	short	EXIT

endfunc		; }

END
