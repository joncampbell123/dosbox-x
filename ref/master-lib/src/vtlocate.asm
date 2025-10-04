; master library - vtext
;
; Description:
;	�e�L�X�g�J�[�\���ʒu���w�肷��
;	( PC/AT BIOS �g�p )
;
; Function/Procedures:
;	void vtext_locate( unsigned x, unsigned y ) ;
;
; Parameters:
;	unsigned x	�J�[�\���̉��ʒu( 0 �` 255 )
;	unsigned y	�J�[�\���̏c�ʒu( 0 �` 255 )
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/08/14 Initial
;	94/01/16 �֐����̕ύX
;		 set_cursor_pos_at -> vtext_locate
;	94/ 4/ 9 Initial: vtlocate.asm/master.lib 0.23
;

	.model SMALL
	include func.inc

	EXTRN Machine_State : WORD

	.CODE

func VTEXT_LOCATE	; vtext_locate() {
	CLI
	add	SP,retsize*2
	pop	AX	; y
	pop	DX	; x
	sub	SP,(retsize+2)*2
	STI

	mov	DH,AL	; dh = y , dl = x

	mov	AX,Machine_State
	test	AX,0010h	; PC/AT ����Ȃ��Ȃ�
	je	short EXIT

	mov	AH,02
	xor	BH,BH
	int	10h
EXIT:
	ret	4
endfunc			; }

END
