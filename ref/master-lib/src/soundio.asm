; master library - PC-9801 - ASM
;
; Description:
;	�T�E���h�{�[�h�̃��W�X�^�ݒ�^�ǂݍ���
;
; Function/Procedures:
;	
;
; Subroutines:
;	SOUND_I:near
;	SOUND_O:near
;
; Parameters:
;	SOUND_O:	BH = reg number
;			BL = value
;	SOUND_I:	BH = reg number
;	SOUND_JOY:	BL = port 0fh�ɃZ�b�g����f�[�^
;
; Break Registers:
;	AL,DX, BH(SOUND_JOY)
;
; Returns:
;	SOUND_I:	AL = read value
;	SOUND_JOY:	AL = port 0eh ����ǂݍ��񂾒l�̔��]
;
; Binding Target:
;	asm
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�T�E���h�h���C�o�Ƃ̋���������邽�߁A���荞�݋֎~��ԂŌĂяo����
;	�������B
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
;	93/ 5/ 2 Initial:soundio.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN js_bexist:WORD

	.CODE
	public SOUND_I,SOUND_O

WAITREADY macro
	local LO
LO:
	in	AL,DX
	test	AL,80h
	jnz	short LO
	endm


SOUND_O	proc near
	mov	DX,188h

	WAITREADY
	mov	AL,BH
	out	DX,AL

	WAITREADY
	inc	DX
	inc	DX
	mov	AL,BL
	out	DX,AL
	ret
SOUND_O	endp

SOUND_I proc near
	mov	DX,188h

	WAITREADY
	mov	AL,BH
	out	DX,AL

	WAITREADY
	inc	DX
	inc	DX
	in	AL,DX
	ret
SOUND_I endp

END
