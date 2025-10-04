; master library - vtext
;
; Description:
;	�e�L�X�g��ʏ����̊J�n/�I��
;
; Functions/Procedures:
;	void vtext_start( void ) ;
;	void vtext_end( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	PC/AT �ł͉��z VRAM �A�h���X��Ԃ��B
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
;	vtext_start:
;		video mode �𔻒肵�� 03 �܂��́A70 �Ȃ�
;		TextVramAdr �ɓK���l��ݒ肵�܂��B
;		����ɂ���� video mode �� 03 �܂��� 70 �̂Ƃ���
;		�����\�����s���܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/ 8/18 Initial
;	93/ 9/21 TextVramSize , TextVramWidth ��ݒ肷��悤�ύX
;	93/10/10 ���������s����O�ɁA�܂� get_machine ���ƁB
;	94/01/16 text_begin -> vtext_start , pc_start
;	94/01/25 call --> _call
;	94/ 4/ 9 Initial: vtstart.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramSize:WORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

	EXTRN VTEXT_WIDTH:CALLMODEL
	EXTRN VTEXT_HEIGHT:CALLMODEL

func VTEXT_START	; vtext_start() {
	_call	VTEXT_WIDTH
	mov	TextVramWidth,AX
	mov	BX,AX
	_call	VTEXT_HEIGHT
	mul	BL
	mov	TextVramSize,AX

	push	DI
	mov	AX,0b800h	; es:di = b800:0000
	mov	ES,AX
	xor	DI,DI
	mov	AH,0feh		; ���z VRAM address �̎擾
	int	10h
	mov	word ptr TextVramAdr,DI
	mov	word ptr TextVramAdr+2,ES

	and	VTextState,not 1	; update���Ă�
	xor	AX,AX
	mov	ES,AX
	mov	AH,0feh
	int	10h		; ���z VRAM address �̎擾
	mov	AX,ES
	test	AX,AX
	jnz	short DONE
	or	VTextState,1	; update���Ă΂Ȃ�
DONE:
	pop	DI

VTEXT_END label CALLMODEL
	ret
endfunc			; }

END
