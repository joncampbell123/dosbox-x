; master library - DOS
;
; Description:
;	DOS •¶š—ñ‚Ì“ü—Í(int 21h, ah=0ah)
;
; Functions/Procedures:
;	int dos_gets(char *buffer, int max);
;
; Parameters:
;	
;
; Returns:
;	“ü—Í‚³‚ê‚½•¶š”
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
;	Kazumi(‰œ“c  m)
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
	max	= (RETSIZE+1)*2		; Å‘å•¶š”‚Í 1 ` 255

	_lds	BX,[BP+buffer]
	mov	AX,[BP+max]
	mov	[BX],AL
	mov	DX,BX
	mov	AH,0ah
	int	21h
	xor	AH,AH
	mov	AL,[BX+1]		; ÀÛ‚É“ü—Í‚³‚ê‚½•¶š”
	_pop	DS
	pop	BP
	ret	(DATASIZE+1)*2
endfunc			; }

END
