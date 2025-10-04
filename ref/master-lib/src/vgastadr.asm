; master library - 
;
; Description:
;	VGAの表示開始アドレスの設定
;
; Function/Procedures:
;	procedure vga_startaddress( address:Word ) ;
;
; Parameters:
;	address: 表示開始オフセットアドレス
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
;	恋塚昭彦
;
; Revision History:
;	93/12/25 Initial: vgastadr.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.CODE

DC_PORT	equ	3d4h

func VGA_STARTADDRESS	; vga_startaddress() {
	mov	BX,SP
	; 引数
	adr = (RETSIZE+0)*2
	mov	BX,SS:[BX+adr]

	mov	DX,03dah	; VGA入力レジスタ1(カラー)
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
