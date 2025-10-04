; master library - MS-DOS
;
; Description:
;	�J�����g�h���C�u�̕ύX
;
; Function/Procedures:
;	int dos_setdrive( int drive ) ;
;
; Parameters:
;	int drive	0=A:, 1=B:..
;			'a', 'A'����
;
; Returns:
;	int	"�ڑ�����Ă���h���C�u�̐�"
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
;	���ˏ��F
;
; Revision History:
;	92/11/17 Initial
;	95/ 2/12 [M0.22k] 'a'��'A'���F��

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
