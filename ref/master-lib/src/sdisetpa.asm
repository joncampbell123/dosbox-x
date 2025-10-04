; master library - PC98
;
; Description:
;	Palettes,PaletteTone�̒l��SDI�ɐݒ肷��B
;
; Function/Procedures:
;	void sdi_set_palettes( int page )
;
; Parameters:
;	int page	�ݒ肷��y�[�W
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
;	SDI���풓���Ă��Ȃ��ꍇ�A�Ȃɂ����܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN SdiExists : WORD

	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD

	.CODE

; void sdi_set_palettes( int page ) ;

func SDI_SET_PALETTES
	cmp	SdiExists,0
	jz	short IGNORE

	mov	BX,SP
	mov	AL,SS:[BX+RETSIZE*2]

	mov	AH,0Bh	; set color mode
	mov	DL,5	; 16/4096color
	int	40h

	mov	AH,0Dh	; set palettes
	mov	BX,offset Palettes
	int	40h

	mov	AH,11h	; set tone
	mov	DL,BYTE PTR PaletteTone
	int	40h

IGNORE:
	ret	2
endfunc

END
