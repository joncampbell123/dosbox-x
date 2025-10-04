; master library - MS-DOS
;
; Description:
;	DOS�����R���\�[��������o�́E���s�ϊ��t��
;
; Function/Procedures:
;	void dos_cputs2( const char * string ) ;
;
; Parameters:
;	char * string	������
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
;	92/11/17 Initial: doscputs.asm
;	93/ 1/16 SS!=DS�Ή�
;	93/ 8/17 Initial: doscpts2.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_CPUTS2		; dos_cputs2() {
	mov	BX,SP
	mov	DX,SI	; save SI
	_push	DS
	_lds	SI,SS:[BX+RETSIZE*2]
	lodsb
	or	AL,AL
	je	short EXIT
CLOOP:	cmp	AL,0ah		; '\n'
	jne	short NOT_LF
	mov	AL,0dh
	int	29h
	mov	AL,0ah
NOT_LF:
	int	29h
	lodsb
	or	AL,AL
	jne	short CLOOP
EXIT:
	_pop	DS
	mov	SI,DX	; restore SI
	ret	DATASIZE*2
endfunc			; }

END
