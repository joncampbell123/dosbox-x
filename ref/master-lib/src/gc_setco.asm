; master library - grcg - PC98V - CINT
;
; Function:
;	void _pascal grcg_setcolor( int mode, int color ) ;
;	void _pascal grcg_off(void) ;
;
; Description:
;	�E�O���t�B�b�N�`���[�W���[�̃��[�h�ݒ肵�A�P�F��ݒ肷��B
;	�E�O���t�B�b�N�`���[�W���[�̃X�C�b�`��؂�B
;
; Parameters:
;	int mode	���[�h�B
;	int color	�F�B0..15
;
; Binding Target:
;	Microsoft-C / Turbo-C / etc...
;
; Running Target:
;	PC-9801V Normal Mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRAPHIC CHAGER
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0�݊��Ȃ�� OK )
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/ 6/ 8 Initial
;	92/ 6/11 gc_setcolor()��"ret"�Ŗ߂��Ă���(^^; -> ret 4 �ɏC��
;		 (Thanks, Mikio)
;	92/ 6/16 TASM�Ή�
;	92/ 7/10 mode reg�ݒ莞�A���荞�݂��֎~
;	93/ 5/ 5 [M0.16] �^�C�����W�X�^�ݒ蒆�܂Ŋ��荞�݂��֎~������[��

	.MODEL SMALL
	.CODE
	include func.inc

func GRCG_SETCOLOR	; {
	mov	BX,SP

	; ����
	mode  = (RETSIZE+1)*2
	color = (RETSIZE+0)*2

	mov	AL,SS:[BX+mode]
	mov	AH,SS:[BX+color]

	mov	DX,007eh
	pushf
	CLI
	out	7ch,AL

	shr	AH,1		; B
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; R
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; G
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; I
	sbb	AL,AL
	out	DX,AL
	popf

	ret	4
endfunc			; }

; void _pascal grcg_off(void) ;
func GRCG_OFF
	xor	AL,AL
	out	7ch,AL
	ret
endfunc

END
