; master library - MS-DOS
;
; Description:
;	DOS�����R���\�[�� �p�X�J��������o��
;
; Function/Procedures:
;	void dos_cputp( const char * passtr ) ;
;
; Parameters:
;	char * passtr	�p�X�J��������
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
;	DOS�̓���f�o�C�X�R�[�� ( int 29h )�𗘗p���Ă��܂��B
;	������ DOS�ł͎g���Ȃ��Ȃ邩������܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/19 Initial
;	93/ 1/16 SS!=DS�Ή�

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_CPUTP
	mov	BX,SP
	mov	DX,SI	; save SI
	_push	DS
	_lds	SI,SS:[BX+RETSIZE*2]
	lodsb
	mov	CH,0
	mov	CL,AL
	jcxz	short EXIT
CLOOP:	lodsb
	int	29h
	loop	short CLOOP
EXIT:
	_pop	DS
	mov	SI,DX	; restore SI
	ret	DATASIZE*2
endfunc

END
