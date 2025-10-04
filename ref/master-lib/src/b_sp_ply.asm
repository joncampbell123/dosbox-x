; master library - BGM
;
; Description:
;	���t���~�߂�
;
; Function/Procedures:
;	int bgm_stop_play(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		����I��
;	BGM_NOT_PLAY		���Ɏ~�܂��Ă���
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
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
;	femy(��  ����)		: �I���W�i���EC�����
;	steelman(���  �T�i)	: �A�Z���u�������
;
; Revision History:
;	93/12/19 Initial: b_sp_ply.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
	EXTRN	_BGM_BELL_ORG:CALLMODEL

func BGM_STOP_PLAY
	cmp	glb.rflg,ON
	jne	short NOTPLAYING
	mov	glb.rflg,OFF
	call	_BGM_BELL_ORG
	xor	AX,AX			; BGM_COMPLETE
	ret
even
NOTPLAYING:
	mov	AX,BGM_NOT_PLAY
	ret
endfunc
END
