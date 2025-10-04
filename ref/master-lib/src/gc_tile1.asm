PAGE 98,120
; graphics - grcg - tile - 1line
;
; Description:
;	�^�C���p�^�[���̐ݒ�(1 line)
;
; Function/Procedures:
;C:	void [_far] _pascal grcg_settile_1line( int mode, long tile ) ;
;
;Pas:	procedure grcg_settile_1line( mode : integer, tile : longint ) ;

; Parameters:
;	int mode	GRCG�̃��[�h�B
;	long tile	�^�C���p�^�[���B���ʃo�C�g����P�o�C�g�P�ʂ�
;			B,R,G,I�v���[���ƂȂ�B
;			0x000000FFL���A�v���[���̂ݑS��ON,����ȊO
;			�S��OFF�̒l�ƂȂ�B
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRCG
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0�݊��Ȃ�� OK )
;
; NOTES:
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grcg_setcolor()
;	grcg_off()
;
; HISTORY:
;	92/ 6/28 Initial

	.186
	.MODEL SMALL
	.CODE
	include func.inc

func GRCG_SETTILE_1LINE
	mov	CX,BP
	mov	BP,SP

	; parameters
	mode  = (retsize+2)*2
	tileh = (retsize+1)*2
	tilel = (retsize+0)*2

	mov	AL,[BP+mode]
	out	7ch,AL
	mov	DX,007eh

	mov	AX,[BP+tilel]
	out	DX,AL		; B
	mov	AL,AH
	out	DX,AL		; R

	mov	AX,[BP+tileh]
	out	DX,AL		; G
	mov	AL,AH
	out	DX,AL		; I

	mov	BP,CX
	ret	6
endfunc

END
