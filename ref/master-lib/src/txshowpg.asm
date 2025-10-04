; master library module
;
; Description:
;	�e�L�X�g��ʂ̕\���y�[�W�ݒ�
;
; Functions/Procedures:
;	void text_showpage( int page ) ;
;
; Parameters:
;	page & 1 ���ǂ��̂����̂���
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	93/ 3/26 Initial: master.lib

	.MODEL SMALL
	include func.inc
	.CODE

func TEXT_SHOWPAGE
	mov	BX,SP
	; ����
	showpage = (RETSIZE+0)*2

	mov	AH,0eh
	mov	DX,SS:[BX+showpage]
	shr	DX,1
	sbb	DX,DX
	and	DX,1000h
	int	18h
	ret	2
endfunc

END
