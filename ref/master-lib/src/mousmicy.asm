; master library - MOUSE - MICKEY
;
; Description:
;	�}�E�X�̈ړ����x���~�b�L�[�^�h�b�g��Őݒ肷��
;
; Function/Procedures:
;	void mouse_setmickey( int cx, int cy ) ;
;
; Parameters:
;	cx,cy	���Əc�́A�}�E�X���W��8�ړ����邽�߂ɕK�v�ȃ}�E�X�J�E���g��
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	�g�p����}�E�X�h���C�o�ɂ��
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
;	���ˏ��F
;
; Revision History:
;	93/ 7/26 Initial: mousmicy.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN mouse_Type:WORD
	EXTRN mouse_ScaleX:WORD
	EXTRN mouse_ScaleY:WORD

	.CODE

func MOUSE_SETMICKEY ; mouse_setmickey() {
	mov	BX,SP
	; ����
	_cx = (RETSIZE+1)*2
	_cy = (RETSIZE+0)*2

	pushf
	CLI
	mov	CX,SS:[BX+_cx]
	mov	mouse_ScaleX,CX
	mov	DX,SS:[BX+_cy]
	mov	mouse_ScaleY,DX

	cmp	mouse_Type,0
	je	short SKIP

	mov	AX,15
	int	33h
SKIP:
	popf
	ret	4
endfunc		; }

END
