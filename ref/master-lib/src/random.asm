; master library - random
;
; Description:
;	�Ȉ՗���
;
; Function/Procedures:
;	function IRand : Integer ;
;
; Parameters:
;	none
;
; Returns:
;	IRand: 0�`32767�̗���
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	Borland C++ 3.1�̃����^�C�����C�u�������Q�l�ɂ��Ă��܂��B
;
;	���̏������Ԃ́APC-9801RA21(Cx486DRx2 20/40MHz)�� 2.1��sec�ł��B
;	���Ȃ݂ɁA��������� Visial C++ 1.0J�� rand()�� 3.8��sec,
;	                     Borland C++3.1          �� 2.9��sec�ł��̂ŁA
;	�W�����C�u�����̂��̂��͍����Ƃ������ƂɂȂ�܂��B
;	�����I�ɂ́ABC3.1�̂��̂����������������ɂ����܂���̂ŁA
;	���S�ɑ�p�ł��܂��B
;
; Assembly Language Note:
;	CX,DX���W�X�^�ƃt���O��j�󂵂܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 1/ 6 Initial: random.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	random_seed:DWORD

	.CODE

	HI_MUL	equ 015ah
	LO_MUL	equ 4e35h


func IRAND	; IRand() {
	mov	AX,LO_MUL
	mul	word ptr random_seed+2
	mov	CX,AX
	mov	AX,HI_MUL
	mul	word ptr random_seed
	add	CX,AX
	mov	AX,LO_MUL
	mul	word ptr random_seed
	add	AX,1
	adc	DX,CX
	mov	word ptr random_seed,AX
	mov	AX,DX
	mov	word ptr random_seed+2,AX
	and	AH,7fh
	ret
endfunc		; }

END
