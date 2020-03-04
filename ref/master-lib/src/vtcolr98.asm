; master library - vtext
;
; Description:
;	PC-9801 用の Text color 属性を PC/AT 用に変換する
;	起動マシンが PC-9801 の場合は、設定した値がそのまま返り値となる。
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
;	PC-9801, PC/AT 以外のマシンの返り値について保証できません。
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	のろ/V(H.Maeda)
;	恋塚
;
; Revision History:
;	93/ 8/25 Initial
;	94/ 4/ 9 Initial: vtcolr98.asm/master.lib 0.23
;	94/ 7/27 [M0.23] 大幅に短縮
;	95/ 3/ 1 [M0.22k] BUGFIX GとRのビットが逆だった


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
	; PC/AT だったら

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
