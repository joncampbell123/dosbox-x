; master library - BGM
;
; Description:
;	�e���|��ύX����
;
; Function/Procedures:
;	int bgm_set_tempo(int tempo);
;
; Parameters:
;	tempo		�ݒ肷��e���|
;
; Returns:
;	BGM_COMPLETE	����I��
;	BGM_EXTENT_ERR	�e���|�̒l���ُ�
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
;	93/12/19 Initial: b_s_temp.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
func BGM_SET_TEMPO
	mov	BX,SP
	tempo	= (RETSIZE+0)*2
	mov	CX,SS:[BX+tempo]
	cmp	CX,TEMPOMIN
	jl	short ILLEGAL
	cmp	CX,TEMPOMAX
	jg	short ILLEGAL

	CLI

	mov	glb.tp,CX
	mov	AX,word ptr glb.clockbase
	mov	DX,word ptr glb.clockbase+2
	div	CX
	mov	glb.tval,AX

	STI

	xor	AX,AX			; BGM_COMPLETE
	ret	2
ILLEGAL:
	mov	AX,BGM_EXTENT_ERR
	ret	2
endfunc
END
