; master library - PC98V
;
; Description:
;	Palettes�̓��e��Tone 100%�Ƃ݂Ȃ��Ď��ۂ̃n�[�h�E�F�A�ɏ������ށB
;
; Function/Procedures:
;	void palette_show100( void ) ;
;
; Parameters:
;	none
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
;	CPU: V30
;
; Notes:
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
;	92/11/25 Initial
;	93/12/ 5 [M0.22] Palettes �� 4bit����8bit�ɂȂ����̂ɑΉ�
;	94/ 5/ 1 [M0.23] ����shift���ĂȂ������̂����

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes : WORD

	.CODE

func PALETTE_SHOW100
	mov	DX,SI	; save SI
	CLD

	mov	SI,offset Palettes
	mov	BX,0
PLOOP:	mov	AL,BL
	out	0a8h,AL	; palette number

	lodsw
	shr	AX,4
	and	AL,0fh
	out	0ach,AL	; r
	mov	AL,AH
	out	0aah,AL	; g
	lodsb
	shr	AL,4
	out	0aeh,AL	; b

	inc	BX
	cmp	BX,16
	jl	short PLOOP

	mov	SI,DX	; restore SI
	ret
endfunc

END
