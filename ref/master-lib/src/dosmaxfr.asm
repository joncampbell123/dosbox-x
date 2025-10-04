; master library - 
;
; Description:
;	DOS �̍ő�t���[�u���b�N�̃p���O���t�T�C�Y�𓾂�
;
; Function/Procedures:
;	unsigned dos_maxfree(void) ;
;	function dos_maxfree : Word ;
;
; Parameters:
;	none
;
; Returns:
;	�ő�t���[�u���b�N�̃p���O���t�T�C�Y
;	0 �Ȃ烁�����R���g���[���u���b�N���j�󂳂�Ă���
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	DOS
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
;	93/11/ 1 Initial: dosmaxfr.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_MAXFREE ; dos_maxfree() {
	mov	AH,48h
	mov	BX,-1
	int	21h
	cmp	AX,8	; �������s��?
	jne	short ERROREXIT
	mov	AX,BX
	clc
	ret
ERROREXIT:	; �����ɗ���Ƃ�����A
		;�������R���g���[���u���b�N���j�󂳂�Ă���ꍇ����
	xor	AX,AX
	stc
	ret
endfunc		; }

END
