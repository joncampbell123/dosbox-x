; master library - DOS
;
; Description:
;	DOS ������̓���(int 21h, ah=0ah)
;
; Functions/Procedures:
;	int dos_gets(char *buffer, int max);
;
; Parameters:
;	
;
; Returns:
;	���͂��ꂽ������
;
; Binding Target:
;	Microsoft-C / Turbo-C
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
;	Kazumi(���c  �m)
;
; Revision History:
;	95/ 1/31 Initial: dosgets.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.CODE

func	DOS_GETS	; dos_gets() {
	push	BP
	mov	BP,SP
	_push	DS

	buffer	= (RETSIZE+2)*2
	max	= (RETSIZE+1)*2		; �ő啶������ 1 �` 255

	_lds	BX,[BP+buffer]
	mov	AX,[BP+max]
	mov	[BX],AL
	mov	DX,BX
	mov	AH,0ah
	int	21h
	xor	AH,AH
	mov	AL,[BX+1]		; ���ۂɓ��͂��ꂽ������
	_pop	DS
	pop	BP
	ret	(DATASIZE+1)*2
endfunc			; }

END
