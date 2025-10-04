; master library - MS-DOS - 30BIOS
;
; Description:
;	�R�O�sBIOS ( (c)lucifer ) ����
;	�s���֌W
;
; Procedures/Functions:
;	unsigned bios30_getline( void ) ;
;	void bios30_setline( int lines ) ;
;
; Parameters:
;	bios30_setline:
;		line: �s�ԂȂ��̂Ƃ��̐ݒ�s��
;
; Returns:
;	bios30_getline:
;		��ʃo�C�g: �s�Ԃ���̂Ƃ��̍s��-1
;		���ʃo�C�g: �s�ԂȂ��̂Ƃ��̍s��-1
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30bios �̃o�[�W������ 0.08�����Â��ƈُ킩������܂���(^^;
;	30bios, TT 1.50 API �̂ǂ�����풓���Ă��Ȃ���� 0 ��Ԃ��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 Initial: b30line.asm/master.lib 0.16
;	93/ 9/13 [M0.21] 30bios_exist -> 30bios_tt_exist
;	93/12/ 9 [M0.22] TT API �Ή�
;	93/12/11 [M0.22] BUGFIX TT API�ł�getline�͒l�� 1������Ă��Ȃ�����
;

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETLINE	; unsigned bios30_getline( void ) {
	_call	BIOS30_TT_EXIST
	and	AL,84h
	jz	short GETLINE_FAILURE
	mov	BL,0
	mov	AX,0ff03h		; 30bios API; get line
	js	short GETLINE_30BIOS
	mov	AX,1809h		; TT 1.50 API; Get TextLine2
	mov	BX,'TT'
	int	18h
	sub	AX,0101h		; ���ꂼ��1������
	ret
GETLINE_30BIOS:
	int	18h
GETLINE_FAILURE:
	ret
endfunc			; }


func BIOS30_SETLINE	; void bios30_setline( int lines ) {
	_call	BIOS30_TT_EXIST
	and	AL,82h		; 30bios API, TT API
	jz	short SETLINE_FAILURE
	mov	AX,0ff03h	; 30bios API; set line
	mov	BX,SP
	mov	BL,SS:[BX+RETSIZE*2]
	js	short SETLINE_30BIOS

	mov	AL,BL
	cmp	AL,20			; TT��20�s�����̐ݒ�͕ʃR�}���h�����c
	jl	short SETLINE_FAILURE	; ExtMode�ݒ��MSB=1, �܂蕉�Ȃ̂�
	mov	AH,18h
	mov	BX,'TT'
SETLINE_30BIOS:
	int	18h
SETLINE_FAILURE:
	ret	2
endfunc			; }

END
