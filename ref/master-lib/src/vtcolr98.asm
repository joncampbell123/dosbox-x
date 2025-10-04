; master library - vtext
;
; Description:
;	PC-9801 �p�� Text color ������ PC/AT �p�ɕϊ�����
;	�N���}�V���� PC-9801 �̏ꍇ�́A�ݒ肵���l�����̂܂ܕԂ�l�ƂȂ�B
;
; Procedures/Functions:
;	int vtext_color_98( int )
;
; Parameters:
;	int PC98_color_attribute
;
; Returns:
;	int PC/AT_color_attribute
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801, PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PC-9801, PC/AT �ȊO�̃}�V���̕Ԃ�l�ɂ��ĕۏ؂ł��܂���B
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	�̂�/V(H.Maeda)
;	����
;
; Revision History:
;	93/ 8/25 Initial
;	94/ 4/ 9 Initial: vtcolr98.asm/master.lib 0.23
;	94/ 7/27 [M0.23] �啝�ɒZ�k
;	95/ 3/ 1 [M0.22k] BUGFIX G��R�̃r�b�g���t������


	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN Machine_State : WORD

	.CODE
PC_AT		equ 010h
TX_REVERSE	equ 4

func VTEXT_COLOR_98	; vtext_color_98() {
	color	= (RETSIZE+0)*2

	mov	BX,BP		; push BP
	mov	BP,SP
	mov	AX,[BP+color]	; color
	mov	BP,BX

	test	Machine_State,PC_AT
	je	short CHANGE_COLOR_EXIT
	; PC/AT ��������

	mov	AH,AL

	shl	AH,1	; G
	rcr	AL,1
	shl	AH,1	; R
	rcl	AL,1
	rcl	AL,1
	shl	AH,1	; B
	rcl	AL,1
	and	AL,7
	or	AL,8	; highlight

	test	AH,TX_REVERSE shl 3
	mov	AH,0
	jz	short CHANGE_COLOR_EXIT
	mov	CL,4
	shl	AL,CL
CHANGE_COLOR_EXIT:
	ret	2
endfunc			; }

END
