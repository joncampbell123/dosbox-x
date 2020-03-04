; master library - PC-9801 - KEY - RESET
;
; Description:
;	キーボードをリセットする
;
; Function/Procedures:
;	void key_reset( void ) ;
;
; Parameters:
;	none
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
;	93/ 8/14 Initial: keyreset.asm/master.lib 0.21
;	95/ 3/29 [M0.22k] BIOSの押し下げマップも消去

	.MODEL SMALL
	include func.inc

	.CODE

func KEY_RESET	; key_reset() {
	pushf
	CLI
	mov	AL,37H	; TXEN = 1, KBDE = 1
	out	43h,AL

	mov	AL,0	; BK set command
	out	41h,AL

	; 15μsec delay
	mov	CX,15*10/6
DELAY15u:
	out	5fh,AL		; 0.6μsec delay
	loop	short DELAY15u

WAIT_SEND:
	in	AL,43h	; 8251(KB) status
	test	AL,4	; TxEMP
	jz	short WAIT_SEND

	mov	AL,16H	; TXEN=0, KBDE=0
	out	43h,AL

	; 438μsec delay
	mov	CX,438*10/6
DELAY438u:
	out	5fh,AL		; 0.6μsec delay
	loop	short DELAY438u

	; BIOS work clear
	push	DI
	CLD
	mov	AX,0
	mov	DI,052ah
	mov	ES,AX
	mov	CX,8
	rep	stosw
	pop	DI
	popf
	ret
endfunc		; }

END
