; master library - MS-DOS
;
; Description:
;	�J�����g�f�B���N�g���̓ǂݎ��
;
; Function/Procedures:
;	int dos_getcwd( int drive, char * buf )
;
; Parameters:
;	int drive	�h���C�u(cur=0, A:=1, B:=2..)
;	char * buf	���ʂ̏������ݐ�
;
; Returns:
;	1 = ����
;	0 = ���s
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
	EXTRN DOS_GETDRIVE:CALLMODEL

func DOS_GETCWD	; dos_getcwd() {
	push	BP
	mov	BP,SP
	push	SI

	; ����
	drive	= (RETSIZE+1+DATASIZE)*2
	buf	= (RETSIZE+1)*2

	mov	AL,[BP+drive]
	or	AL,AL
	jnz	short GO
	_call	DOS_GETDRIVE
	inc	AL
GO:
	and	AL,1fh
	mov	DL,AL
	add	AL,'@'
	mov	AH,':'
	_push	DS
	_lds	BX,[BP+buf]
	mov	[BX],AX
	mov	byte ptr [BX+2],'\'
	add	BX,3
	mov	AH,47h
	mov	SI,BX
	int	21h
	_pop	DS

	pop	SI
	sbb	AX,AX
	inc	AX
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
