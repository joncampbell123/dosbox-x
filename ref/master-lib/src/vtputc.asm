; master library - vtext
;
; Description:
;	�e�L�X�g��ʂւ̕����̏�������
;	�����Ȃ�
;
; Function/Procedures:
;	void vtext_putc( unsigned x, unsigned y, unsigned chr ) ;
;
; Parameters:
;	unsigned x	���[�̍��W
;	unsigned y	��[�̍��W
;	unsigned chr	����(ANK, JIS, SHIFT-JIS�R�[�h)
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
;	94/ 4/ 9 Initial: vtxputc.asm/master.lib 0.23
;	94/ 4/13 [M0.23] JIS�R�[�h�Ή�
;	94/12/21 [M0.23] JIS->SJIS�ϊ������̒u������

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramWidth:WORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

func VTEXT_PUTC	; vtext_putc() {
	mov	BX,SP
	push	DI

	; ����
	x	= (RETSIZE+2)*2
	y	= (RETSIZE+1)*2
	chr	= (RETSIZE+0)*2

	les	DI,TextVramAdr
	mov	AX,SS:[BX+y]	; �A�h���X�v�Z
	mul	TextVramWidth
	add	AX,SS:[BX+x]
	shl	AX,1
	add	DI,AX

	mov	CX,1

	mov	AX,SS:[BX+chr]	; AL = chr(L)
	cmp	AH,0
	jle	short NOT_JIS

	; JIS �� SHIFT JIS
	sub	AX,0de82h
	rcr	AH,1
	jb	short SKIP0
		cmp	AL,0deh
		sbb	AL,05eh
SKIP0:	xor	AH,20h

NOT_JIS:
	test	byte ptr VTextState,01
	jnz	short DIRECT

	mov	CX,1
	test	AH,AH
	jz	short ANKTEST
	inc	CX
	cmp	ES:[DI+2],AL
	je	short	ANK2
	mov	ES:[DI+2],AL
	mov	AL,AH
	jmp	short WRITE
ANK2:
	mov	AL,AH
ANKTEST:
	cmp	ES:[DI],AL
	je	short NO_REF
WRITE:
	mov	ES:[DI],AL
	mov	AH,0ffh
	int	10h
NO_REF:
	pop	DI
	ret	6
	EVEN

DIRECT:
	test	AH,AH
	jz	short DANK
	mov	ES:[DI+2],AL
	mov	AL,AH
DANK:
	mov	ES:[DI],AL
	pop	DI
	ret	6
endfunc		; }

END
