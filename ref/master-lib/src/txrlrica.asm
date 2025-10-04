; master library - text - scroll - right - character - attribute
;
; Description:
;	�e�L�X�g��ʂ̉E�X�N���[��
;	��������
;
; Function/Procedures:
;	void text_roll_right_ca(unsigned fillchar, unsigned filatr);
;
; Parameters:
;	fillchar	�V�������ꂽ�����𖄂߂镶��
;	fillatr		�V����
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
;	Kazumi
;
; Revision History:
;	94/ 3/30 Initial: txrlrica.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD

	.CODE

func TEXT_ROLL_RIGHT_CA	; text_roll_right_ca() {
	push	BP
	mov	BP,SP
	; ����
	fillchar = (RETSIZE+2)*2
	fillatr = (RETSIZE+1)*2
	push	SI
	push	DI

	std
	mov	DI,text_RollTopOff
	mov	DX,text_RollHeight
	inc	DX
	mov	AX,text_RollWidth
	dec	AX
	mov	BX,AX
	shl	BX,1
	add	DI,BX
	lea	SI,[DI-2]
	mov	ES,TextVramSeg
LOOPY:
	mov	CX,AX
	rep	movs word ptr ES:[DI],word ptr ES:[SI]
	lea	SI,[SI+BX+2000h]
	lea	DI,[DI+BX+2000h]
	mov	CX,AX
	rep	movs word ptr ES:[DI],word ptr ES:[SI]
	lea	SI,[SI+BX-2000h+160]
	lea	DI,[DI+BX-2000h+160]
	dec	DX
	jg	short LOOPY

	cld
	mov	DI,text_RollTopOff
	mov	SI,DI
	mov	CX,text_RollHeight
	inc	CX
	mov	DX,CX
	mov	AX,[BP+fillchar]
FILLC:	stosw
	add	DI,158			;(80 - 1) * 2
	loop	FILLC
	mov	CX,DX
	lea	DI,[SI+2000h]
	mov	AX,[BP+fillatr]
FILLA:	stosw
	add	DI,158
	loop	FILLA

	pop	DI
	pop	SI
	pop	BP
	ret	4
endfunc			; }

END
