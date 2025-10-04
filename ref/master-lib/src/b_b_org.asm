; master library - BGM
;
; Description:
;
;
; Function/Procedures:
;	void _bgm_bell_org(void);
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
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
;	femy(淀  文武)		: オリジナル・C言語版
;	steelman(千野  裕司)	: アセンブリ言語版
;	恋塚
;
; Revision History:
;	93/12/19 Initial: b_b_org.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 4/11 [M0.23] AT互換機対応

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA

	EXTRN Machine_State:WORD

	.CODE
func _BGM_BELL_ORG
	test	Machine_State,10h
	jnz	short PCAT

PC98:
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[0501H],80h
	mov	BX,TVAL8ORG/2
	jnz	short CLOCK8MHZ
	mov	BX,TVAL10ORG/2
CLOCK8MHZ:
	;タイマカウント値設定
	mov	DX,BEEP_CNT
	mov	AL,BL
	out	DX,AL			; 98
	mov	AL,BH
	out	DX,AL			; 98
	;ビープOFF
	mov	AL,BEEP_OFF
	out	BEEP_SW,AL		; 98
	ret
PCAT:
	;ビープOFF
	in	AL,61h
	and	AL,not 3
	out	61h,AL			; AT
	ret
endfunc
END
