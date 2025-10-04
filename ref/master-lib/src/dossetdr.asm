; master library - MS-DOS
;
; Description:
;	カレントドライブの変更
;
; Function/Procedures:
;	int dos_setdrive( int drive ) ;
;
; Parameters:
;	int drive	0=A:, 1=B:..
;			'a', 'A'も可
;
; Returns:
;	int	"接続されているドライブの数"
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
;	95/ 2/12 [M0.22k] 'a'や'A'も認識

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_SETDRIVE
	mov	BX,SP
	mov	AH,0eh
	mov	DL,SS:[BX+RETSIZE*2]
	cmp	DL,26
	adc	DL,-1
	and	DL,1fh
	int	21h
	mov	AH,0
	ret	2
endfunc

END
