; master library - PC-9801 - mouse
;
; Description:
;	�}�E�X���荞�݂ɂ��ȈՊJ�n/�I���ݒ�
;
; Function/Procedures:
;	void mouse_istart( int blc, int whc ) ;
;	void mouse_iend( void ) ;
;
; Parameters:
;	blc,whc		�}�E�X�J�[�\���̍��F�A���F�R�[�h
;
; Returns:
;	�Ȃ�
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
;	�J�[�\���̈ړ��A�Ǘ��� cursor_show/hide/moveto���A
;	�`��ݒ�� cursor_pattern���g�p���ĉ������B
;
;	mouse_vstart�͈ȉ��̐ݒ�����܂��B
;		cursor�\�����	��\��
;		cursor�`��	���
;		���荞�ݎ��	mouse���荞��
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
;	93/ 5/ 3 Initial:mousei.asm/master.lib 0.16
;	93/ 7/26 [M0.20] BUGFIX �����ĂȂ�����:-<

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	cursor_Arrow:WORD
	EXTRN	mouse_EventRoutine:DWORD
	EXTRN	mouse_EventMask:WORD
	EXTRN	mouse_X:WORD
	EXTRN	mouse_Y:WORD

	.CODE
	EXTRN	CURSOR_INIT:CALLMODEL
	EXTRN	CURSOR_PATTERN:CALLMODEL
	EXTRN	MOUSE_PROC_INIT:CALLMODEL
	EXTRN	MOUSE_PROC:FAR
	EXTRN	MOUSE_INT_START:CALLMODEL
	EXTRN	MOUSE_INT_END:CALLMODEL
	EXTRN	CURSOR_MOVETO:CALLMODEL
	EXTRN	CURSOR_HIDE:CALLMODEL

MOUSE_MOVE equ 4

mouseint proc far
	push	mouse_X
	push	mouse_Y
	call	CURSOR_MOVETO
	ret
mouseint endp

func 	MOUSE_ISTART	; mouse_istart() {
	push	BP
	mov	BP,SP
	; ����
	blc	= (RETSIZE+2)*2
	whc	= (RETSIZE+1)*2

	call	CURSOR_INIT
	mov	AH,0
	mov	AL,byte ptr cursor_Arrow
	push	AX
	mov	AL,byte ptr cursor_Arrow+1
	push	AX
	push	[BP+blc]
	push	[BP+whc]
	push	DS
	mov	AX,offset cursor_Arrow+2
	push	AX
	call	CURSOR_PATTERN

	call	MOUSE_PROC_INIT

	push	mouse_X
	push	mouse_Y
	call	CURSOR_MOVETO

	mov	word ptr mouse_EventRoutine,offset mouseint
	mov	word ptr mouse_EventRoutine+2,CS
	mov	word ptr mouse_EventMask,MOUSE_MOVE

	push	CS
	mov	AX,offset MOUSE_PROC
	push	AX
	xor	AX,AX
	push	AX
	call	MOUSE_INT_START
	pop	BP
	ret	4
endfunc		; }

func 	MOUSE_IEND	; mouse_iend() {
	call	CURSOR_HIDE
	call	MOUSE_INT_END
	ret
endfunc		; }

END
