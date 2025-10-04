; master library - PC/AT
;
; Description:
;	�r�[�v���̎��g���̐ݒ�
;
; Function/Procedures:
;	void vbeep_freq( unsigned freq ) ;
;
; Parameters:
;	unsigned freq	���g��(19�`65535Hz)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
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
;	94/ 4/11 Initial: vbeepfrq.asm/master.lib 0.23


	.MODEL SMALL

	include func.inc

	.CODE

func VBEEP_FREQ	; vbeep_freq() {
	mov	BX,SP

	; ������
	freq = RETSIZE*2

	mov	AL,0b6h		; mode counter#2,  low-high, square rate, bin
	out	43h,AL

	mov	DX,0012h 	; 1193.18K(AT�݊��@)
	mov	AX,34dch
	div	word ptr SS:[BX+freq]		; AX = setdata
	out	42h,AL
	mov	AL,AH
	out	42h,AL
	ret	2
endfunc		; }

END
