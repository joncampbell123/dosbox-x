; master library - MS-DOS
;
; Description:
;	DOSのブレーク禁止 ( int 23h )
;	DOSのブレーク許可 ( int 23h )
;
; Function/Procedures:
;	void dos_ignore_break(void) ;
;	void dos_accept_break(void) ;
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
;	92/11/17 Initial
;	93/ 3/23 for tiny model
;	93/ 4/19 dos_accept_break追加

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_IGNORE_BREAK
	push	DS
	push	CS
	pop	DS
	mov	DX,offset IGNORE
	mov	AX,2523h
	int	21h
	pop	DS
	ret
ACCEPT:	stc
IGNORE:	retf
endfunc

func DOS_ACCEPT_BREAK
	push	DS
	push	CS
	pop	DX
	mov	DX,offset ACCEPT
	mov	AX,2523h
	int	21h
	pop	DS
	ret
endfunc

END
