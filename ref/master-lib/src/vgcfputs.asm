; master library - VGA 16Color
;
; Description:
;	������̕`��
;
; Functions/Procedures:
;	void vgc_font_puts(int x, int y, int step, const char * _str);
;
; Parameters:
;	x,y    �ŏ��̕����̍�����W
;	step   �S�p�����P�ʂł� 1�����̐i�ݗ�(16�Ō��ԂȂ�)�B
;	       ���p�͔���(�����_�ȉ��؂�̂�)�Ŏg�p�����B
;	_str   ������
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 186
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
;	���ˏ��F
;
; Revision History:
;	95/ 2/ 1 Initial: vgcfputs.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN	VGC_KANJI_PUTC:CALLMODEL
	EXTRN	VGC_BFNT_PUTC:CALLMODEL

func VGC_FONT_PUTS	; vgc_font_puts() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ������
	x    = (RETSIZE+3+DATASIZE)*2
	y    = (RETSIZE+2+DATASIZE)*2
	step = (RETSIZE+1+DATASIZE)*2
	_str = (RETSIZE+1)*2

	mov	SI,[BP+_str]
	mov	DI,[BP+x]
	CLD

STRLOOP:
	_mov	ES,[BP+_str+2]
    s_ <lodsb>
    l_ <lods	byte ptr ES:[SI]>
	cmp	AL,0
	jz	short OWARI

	test	AL,0e0h
	jns	short ANK	; 00�`7f = ANK
	jp	short ANK	; 80�`9f, e0�`ff = ����
KANJI:
	mov	AH,AL
    s_ <lodsb>
    l_ <lods	byte ptr ES:[SI]>
	cmp	AL,0
	jz	short OWARI

	push	DI
	push	[BP+y]
	push	AX
	call	VGC_KANJI_PUTC
	add	DI,[BP+step]
	jmp	short STRLOOP
	EVEN

ANK:
	mov	AH,0
	push	DI
	push	[BP+y]
	push	AX
	call	VGC_BFNT_PUTC
	mov	AX,[BP+step]
	shr	AX,1
	add	DI,AX
	jmp	short STRLOOP

OWARI:
	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc		; }

END
