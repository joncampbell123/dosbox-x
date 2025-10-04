; master library - PC98 - MSDOS
;
; Description:
;	�e�L�X�g�J�[�\���ʒu���w�肷��( NEC MS-DOS�g���t�@���N�V�����g�p )
;
; Function/Procedures:
;	void text_locate( unsigned x, unsigned y ) ;
;
; Parameters:
;	unsigned x	�J�[�\���̉��ʒu( 0 �` 79 )
;	unsigned y	�J�[�\���̏c�ʒu( 0 �` 24 )
;
; Returns:
;	none
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
;	���ˏ��F
;
; Revision History:
;	92/11/15 Initial
;	92/11/24 MS-DOS workarea�X�V
;	92/12/02 ���Ȃɖ��ʂȂ��Ƃ���Ă񂾂�

	.model SMALL
	include func.inc
	.CODE

func TEXT_LOCATE
	CLI
	add	SP,retsize*2
	pop	AX	; y
	pop	DX	; x
	sub	SP,(retsize+2)*2
	STI

	mov	DH,AL
	mov	AH,3
	mov	CL,10h
	int	0DCh
	ret	4
endfunc

END
