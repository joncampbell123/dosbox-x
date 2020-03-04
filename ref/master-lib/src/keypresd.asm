; master library - MS-DOS - KEY
;
; Description:
;	キーの押下検査
;
; Function/Procedures:
;	int key_pressed(void) ;
;
; Parameters:
;	none
;
; Returns:
;	0 = キーが押されていない
;	1 = キーが押された
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
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
;	恋塚昭彦
;
; Revision History:
;	93/ 8/17 Initial: keypresd.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN key_back_buffer:WORD
	.CODE

func KEY_PRESSED	; key_pressed() {
	mov	AX,1
	cmp	key_back_buffer,AX
	jnb	short RETURN

	mov	AH,0Bh
	int	21h
	and	AX,1
RETURN:
	ret
endfunc		; }

END
