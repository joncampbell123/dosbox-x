; master library - 
;
; Description:
;	VGA�̕\���J�n�A�h���X�̐ݒ�
;
; Function/Procedures:
;	procedure vga_startaddress( address:Word ) ;
;
; Parameters:
;	address: �\���J�n�I�t�Z�b�g�A�h���X
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
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
;	93/12/25 Initial: vgastadr.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

DC_PORT	equ	3d4h

func VGA_STARTADDRESS	; vga_startaddress() {
	mov	BX,SP
	; ����
	adr = (RETSIZE+0)*2
	mov	BX,SS:[BX+adr]

	mov	DX,03dah	; VGA���̓��W�X�^1(�J���[)
WAITVSYNC:
	in	AL,DX
	test	AL,8
	jz	short WAITVSYNC

	mov	DX,DC_PORT
	mov	AL,0ch
	mov	AH,BH
	out	DX,AX

	inc	AL
	mov	AH,BL
	out	DX,AX
	ret	2
endfunc		; }

END
