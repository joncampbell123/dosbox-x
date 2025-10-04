; master library - DOS
;
; Description:
;	�h���C�u�̗e�ʂ𓾂�
;
; Functions/Procedures:
;	int dos_get_driveinfo(int drive, unsigned *cluster, unsigned *sector, unsigned *bytes);
;
; Parameters:
;	drive    0=�J�����g,  1 or 'a' or 'A'=A:
;	cluster  �f�B�X�N�S�̂̃N���X�^���̏������ݐ�
;	sector   �N���X�^������̃Z�N�X�^���̏������ݐ�
;	bytes    �Z�N�^������̃o�C�g���̏������ݐ�
;
; Returns:
;	0   ����
;	-1  �G���[
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
;	95/ 1/31 Initial: dosdrive.asm/master.lib 0.23
;	95/ 2/11 [M0.22k] function 1ch��36h�ɕύX
;	95/ 2/12 [M0.22k] 'a'��'A'���F��

	.MODEL SMALL

	include func.inc

	.CODE

func	DOS_GET_DRIVEINFO	; dos_get_driveinfo() {
	push	BP
	mov	BP,SP

	drive	= (RETSIZE+DATASIZE*3+1)*2
	cluster	= (RETSIZE+DATASIZE*2+1)*2
	sector	= (RETSIZE+DATASIZE+1)*2
	bytes	= (RETSIZE+1)*2

	mov	AH,36h
	mov	DX,[BP+drive]
	and	DX,1fh
	int	21h
	_push	DS
	xor	AH,AH
	_lds	BX,[BP+cluster]
	mov	[BX],DX
	_lds	BX,[BP+sector]
	mov	[BX],AX
	_lds	BX,[BP+bytes]
	mov	[BX],CX
	_pop	DS
	cmp	AX,-1
	je	short error
	xor	AX,AX
	EVEN
error:
	pop	BP
	ret	(DATASIZE*3+1)*2
endfunc				; }

END
