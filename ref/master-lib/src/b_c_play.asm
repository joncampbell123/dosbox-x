; master library - BGM
;
; Description:
;	���t���ĊJ����
;
; Function/Procedures:
;	int bgm_cont_play(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		�ĊJ����
;	BGM_NOT_STOP		���ɉ��t��
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
;	93/12/19 Initial: b_c_play.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
func BGM_CONT_PLAY
	cmp	glb.mcnt,0
	je	short NOWPLAYING
	cmp	glb.rflg,OFF
	jne	short NOWPLAYING
	cmp	glb.music,ON
	jne	short NOWPLAYING
	mov	glb.rflg,ON
	xor	AX,AX			;BGM_COMPLETE
	ret
even
NOWPLAYING:
	mov	AX,BGM_NOT_STOP
	ret
endfunc
END
