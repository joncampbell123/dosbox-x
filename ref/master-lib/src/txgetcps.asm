; master library - PC-9801
;
; Description:
;	�e�L�X�g��ʂ̃J�[�\���ʒu�擾
;
; Function/Procedures:
;	long text_getcurpos( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	long	HIWORD(DX) = y���W( 0 = ��[ )
;		LOWORD(AX) = x���W( 0 = ���[ )
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
;	NEC MS-DOS�̃��[�N�G���A�𒼐ڎQ�Ƃ��Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/20 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func TEXT_GETCURPOS
	xor	AX,AX
	mov	ES,AX
	mov	DX,AX
	mov	AL,ES:[071Ch]
	mov	DL,ES:[0710h]
	ret
endfunc

END
