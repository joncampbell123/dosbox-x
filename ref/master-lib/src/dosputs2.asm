; master library - MS-DOS
;
; Description:
;	標準出力に文字列を出力する・改行変換付き
;
; Function/Procedures:
;	void dos_puts2( const char * string ) ;
;
; Parameters:
;	char * string	文字列
;
; Returns:
;	none
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial: doscputs.asm
;	93/ 1/16 SS!=DS対応
;	93/ 8/17 Initial: doscpts2.asm/master.lib 0.21
;	93/12/26 bugfix

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTS2		; dos_puts2() {
	mov	BX,SP
	mov	CX,SI	; save SI
	_push	DS
	_lds	SI,SS:[BX+RETSIZE*2]
	lodsb
	or	AL,AL
	je	short EXIT
	mov	AH,2
CLOOP:	cmp	AL,0ah		; '\n'
	jne	short NOT_LF
	mov	DL,0dh
	int	21h
	mov	AL,0ah
NOT_LF:
	mov	DL,AL
	int	21h
	lodsb
	or	AL,AL
	jne	short CLOOP
EXIT:
	_pop	DS
	mov	SI,CX	; restore SI
	ret	DATASIZE*2
endfunc			; }

END
