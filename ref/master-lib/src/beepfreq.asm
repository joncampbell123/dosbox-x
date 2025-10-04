; master library - PC98
;
; Description:
;	�r�[�v���̎��g���̐ݒ�
;
; Function/Procedures:
;	void beep_freq( unsigned freq ) ;
;
; Parameters:
;	unsigned freq	���g��(38�`65535Hz), ����l�� 2000Hz
;
; Returns:
;	none
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
;	�͈͊O�̒l����͂���ƁA�O���Z���s���G���[����������̂�
;	���ӂ��Ă��������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/24 Initial
;	93/ 6/25 [M0.19] �����k��
;	93/ 9/15 [M0.21] 5MHz/8MHz�n����� BIOS���[�N�ǂݎ��ɕύX
;	93/11/30 [M0.22] AT
;	94/ 4/11 [M0.23] AT�p�� vbeepfrq.asm�ɕ���


	.MODEL SMALL

	include func.inc

	.CODE

func BEEP_FREQ	; beep_freq() {
	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h

	mov	BX,SP

	; ������
	freq = RETSIZE*2

	mov	DX,001eh 	; 1996.8K(8MHz�n)
	mov	AX,7800h

	jnz	short EIGHTMEGA

	mov	DX,0025h	; 2457.6K(10MHz�n)
	mov	AX,8000h

EIGHTMEGA:
	div	word ptr SS:[BX+freq]		; AX = setdata
	mov	DX,3fdbh

	out	DX,AL
	mov	AL,AH
	out	DX,AL
	ret	2
endfunc		; }

END
