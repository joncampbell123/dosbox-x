; master library - PC-9801
;
; Description:
;	PC-9801�� KEY BIOS ��p���āA�L�[�o�b�t�@�̐擪�̒l�����o��
;
; Function/Procedures:
;	unsigned key_wait_bios(void) ;
;	unsigned key_sense_bios(void) ;
;
; Parameters:
;	none
;
; Returns:
;	���8bit:�L�[�R�[�h
;	����8bit:�L�[�f�[�^(�L�����N�^�R�[�h)
;
;	key_wait_bios:	�L�[���͂�������Ή������܂ő҂��A�o�b�t�@����
;			��菜���B
;	key_sense_bios:	�L�[���͂�������΁A0��Ԃ��B�L�[���͂�����ꍇ��
;			�o�b�t�@�̍X�V�͍s��Ȃ��B
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
;	93/ 8/17 Initial: keybios.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.CODE

func KEY_WAIT_BIOS	; key_wait_bios() {
	mov	AH,0
	int	18h
	ret
endfunc		; }

func KEY_SENSE_BIOS	; key_sense_bios() {
	mov	AH,1
	int	18h
	shr	BH,1
	sbb	BX,BX
	and	AX,BX
	ret
endfunc		; }

END
