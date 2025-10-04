; Hiper library
;
; Description:
;	�N�������}�V�����𓾂�B
;
; Procedures/Functions:
;	int get_machine( void );
;
; Parameters:
;
; Returns:
;          bit 15 - 8 : 0 : (reserved)
;	   bit	 54   :
;		 00   : unknown machine
;		 01   : PC/AT or compatible
;		 10   : PC-9801
;		 11   : unknown machine(���肦�Ȃ�)
;	   bit   3210 : bit 4 �� 1 �̂Ƃ�
;		 000x : PS/55
;		 001x : DOS/V
;		 010x : AX
;		 011x : J3100
;		 100x : DR-DOS
;		 101x : MS-DOS/V
;		 110x : (reserved)
;		 111x : (reserved)
;		 xxx0 : Japanese mode
;		 xxx1 : English mode
;	   bit  83210 : bit 5 �� 1 �̂Ƃ�
;		xxxx0 : DeskTop
;		xxx0x : NEC	#0.07 �ȍ~
;		xxx1x : EPSON	#0.07 �ȍ~
;		x00xx : Normal mode only    (400 lines)
;		x01xx : MATE mode supported (480 lines)
;		x1xxx : High-resolustion mode
;
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	PC-9801, PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PC-9801, PC/AT �ȊO�̃}�V���̕Ԃ�l�ɂ��ĕۏ؂ł��܂���B
;	FM-R �ł́A�\������炵������(^_^;
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	�̂�/V(H.Maeda)
;
; Revision History:
;	93/ 7/19 Initial: Hiper.lib
;	93/07/29 add PC-9821 check
;	93/08/10 add PC-98 color/mono check ?
;		 & _get_machine -> get_machine
;	93/08/25 98 MATE check bugfix
;	93/09/19 ret ������... (^_^;
;	93/10/06 si ���󂵂Ă���(^_^;
;	93/10/22 Mono&Color �`�F�b�N�p�~�ANEC, EPSON �`�F�b�N�ǉ�
;		 �܂��AEPSON note ���`�F�b�N�\
;	93/11/ 3 AX check �ŁA0040:00e0 �`��
;		 16 byte ���� 12 byte check �ɕύX
;	93/12/11 Initial: getmachi.asm/master.lib 0.22 (from hiper.lib)
;	94/01/05 get_machine() ���Aget_machine_at()�Aget_machine_98() �ɕ���
;		 ����ɔ����A���ꂼ��Agetmachi.asm�Agetmacat.asm�Agatmac98.asm
;		 �Ƀt�@�C���𕪗��B
;	95/ 2/21 �e�@�핔����jump���Ă����̂�call�ɕύX���A�߂��Ă�����
;		 DOS BOX���������悤�ɒǉ��B
;	95/ 3/ 8 get_machine_dosbox(getmacdb.asm)�ɕ����B���̂���
;		call�ɂ��Ă����̂�jump�ɖ߂����B


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN	Machine_State:WORD		; machine.asm

FMR	EQU	01000000b

	.CODE

	EXTRN	CHECK_MACHINE_FMR:CALLMODEL	; getmacfm.asm
	EXTRN	GET_MACHINE_98:CALLMODEL	; getmac98.asm
	EXTRN	GET_MACHINE_AT:CALLMODEL	; getmacat.asm

func GET_MACHINE	; get_machine(void)
	_call	CHECK_MACHINE_FMR
	jz	short FMR_RET
	mov	ah,0fh
	int	10h	; get text mode (DOS/V)
	cmp	ah,0fh
	je	GET_MACHINE_98
	jmp	GET_MACHINE_AT
FMR_RET:
	mov	AX,FMR
	ret
endfunc

END
