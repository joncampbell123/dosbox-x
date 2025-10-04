; master library - PC-9801 - mouse
;
; Description:
;	�O���}�E�X�h���C�o�ȈՊJ�n/�I���ݒ�
;
; Function/Procedures:
;	int mousex_istart( int blc, int whc ) ;
;	void mousex_iend( void ) ;
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
;	PC-9801, PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�J�[�\���̕\���Ǘ��� cursor_show/hide ���A
;	�J�[�\���̈ړ��� mousex_cmoveto ���A
;	�`��ݒ�� cursor_pattern ���g�p���ĉ������B
;
;	mouse_vstart�͈ȉ��̐ݒ�����܂��B
;		cursor�\�����	��\��
;		cursor�`��	���
;		���荞�ݎ��	�O���}�E�X�h���C�o�̃C�x���g���荞��
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
;	93/ 7/26 Initial:mousei.asm/master.lib 0.20
;	94/ 5/25 [M0.23] 98�ȊO�Ȃ�����}�E�X�h���C�o���g��Ȃ�
;	94/ 7/ 2 [M0.23] BUGFIX, 98�ȊO�ł�mouse_proc_init(98�ˑ�)���Ă�ł�
;	94/ 7/ 8 [M0.23] BUGFIX, mousex_istart(), stack������Ă�

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	cursor_Arrow:WORD
	EXTRN	mouse_EventRoutine:DWORD
	EXTRN	mouse_EventMask:WORD
	EXTRN	mouse_X:WORD
	EXTRN	mouse_Y:WORD
	EXTRN	mouse_Type:WORD
	EXTRN	Machine_State:WORD

	.CODE
	EXTRN	CURSOR_INIT:CALLMODEL
	EXTRN	CURSOR_PATTERN:CALLMODEL
	EXTRN	MOUSE_PROC_INIT:CALLMODEL
	EXTRN	MOUSE_PROC:FAR
	EXTRN	MOUSE_INT_START:CALLMODEL
	EXTRN	MOUSE_INT_END:CALLMODEL
	EXTRN	CURSOR_MOVETO:CALLMODEL
	EXTRN	CURSOR_HIDE:CALLMODEL
	EXTRN	MOUSEX_START:CALLMODEL
	EXTRN	MOUSEX_END:CALLMODEL

MOUSE_MOVE equ 4

mouseint proc far
	push	mouse_X
	push	mouse_Y
	call	CURSOR_MOVETO
	ret
mouseint endp

func MOUSEX_ISTART	; {
	push	BP
	mov	BP,SP
	; ����
	blc	= (RETSIZE+2)*2
	whc	= (RETSIZE+1)*2

	call	CURSOR_INIT
	mov	AX,cursor_Arrow
	xor	BX,BX
	xchg	AH,BL
	push	AX	; x
	push	BX	; y
	push	[BP+blc]
	push	[BP+whc]
	push	DS
	mov	AX,offset cursor_Arrow
	push	AX
	call	CURSOR_PATTERN

	mov	word ptr mouse_EventRoutine,offset mouseint
	mov	word ptr mouse_EventRoutine+2,CS
	mov	word ptr mouse_EventMask,MOUSE_MOVE

	call	MOUSEX_START
	test	AX,AX
	jnz	short START_OK

	test	Machine_State,20h	; PC98
	jz	short START_OK		; 98�ȊO�Ȃ�}�E�X�h���C�o���Ȃ��Ǝ��s

	CALL	MOUSE_PROC_INIT

	mov	word ptr mouse_EventRoutine,offset mouseint
	mov	word ptr mouse_EventRoutine+2,CS
	mov	word ptr mouse_EventMask,MOUSE_MOVE

	push	CS
	mov	AX,offset MOUSE_PROC
	push	AX
	xor	AX,AX
	push	AX
	call	MOUSE_INT_START
	xor	AX,AX
START_OK:
	pop	BP
	ret	4
endfunc		; }

func 	MOUSEX_IEND	; {
	call	CURSOR_HIDE
	cmp	mouse_Type,0
	jne	short XEND
	test	Machine_State,20h	; PC98
	jz	short XRET		; 98�ȊO�Ȃ�Ȃɂ�����
	call	MOUSE_INT_END
	ret
XEND:
	call	MOUSEX_END
XRET:
	ret
endfunc		; }

END
