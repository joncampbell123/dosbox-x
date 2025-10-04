; master library - vtext
;
; Description:
;	�e�L�X�g��ʂւ̕����̏�������
;	������
;
; Function/Procedures:
;	void vtext_putca( unsigned x, unsigned y, unsigned chr, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	���[�̍��W
;	unsigned y	��[�̍��W
;	unsigned chr	����(ANK, JIS, SHIFT-JIS�R�[�h)
;	unsigned atrb	����
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
;	�������Ȃ������R�[�h���^����ꂽ�ꍇ�A�������ȕ������\������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/10/11 Initial
;	93/11/ 7 VTextState ����
;	94/ 4/ 9 Initial: vtxputca.asm/master.lib 0.23
;	94/ 4/13 [M0.23] JIS�R�[�h�Ή�

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

func VTEXT_PUTCA	; vtext_putca() {
	mov	CX,BP
	mov	BP,SP
	push	DI

	; ����
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	chr	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	les	DI,TextVramAdr
	mov	AX,[BP + y]	; �A�h���X�v�Z
	mul	TextVramWidth
	add	AX,[BP + x]
	shl	AX,1
	add	DI,AX

	mov	AX,[BP + chr]	; AL = chr(L)
	cmp	AH,0
	jle	short NO_JIS

	; JIS �� SHIFT JIS
	add	AX,0ff1fh
	shr	AH,1
	jnc	short skip0
	add	AL,5eh
skip0:
	cmp	AL,7fh
	sbb	AL,-1
	cmp	AH,2eh
	jbe	short skip1
	add	AH,40h
skip1:
	add	AH,71h

NO_JIS:

	mov	DX,[BP + atrb]
	xchg	DL,AH		; DL = chr(H), AH = atrb
	mov	BP,CX

	test	byte ptr VTextState,01
	jnz	short DIRECT

	mov	CX,1
	test	DL,DL
	jz	short ANKTEST
	inc	CX
	cmp	ES:[DI+2],AX
	je	short	ANK2
	mov	ES:[DI+2],AX
	mov	AL,DL
	jmp	short WRITE
ANK2:
	mov	AL,DL
ANKTEST:
	cmp	ES:[DI],AX
	je	short NO_REF
WRITE:
	mov	ES:[DI],AX
	mov	AH,0ffh
	int	10h
NO_REF:
	pop	DI
	ret	8
	EVEN

DIRECT:
	test	DL,DL
	jz	short DANK
	mov	ES:[DI+2],AX
	mov	AL,DL
DANK:
	mov	ES:[DI],AX
	pop	DI
	ret	8
endfunc			; }

END
