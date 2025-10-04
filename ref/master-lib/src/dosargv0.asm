; master library - DOS
;
; Description:
;	�N���p�X���𓾂�
;
; Function/Procedures:
;	void dos_get_argv0( char * argv0 ) ;
;
; Parameters:
;	argv0	�����p�X�����i�[�����
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS 3.1 or later ( 2.1�ł��������Ǐ�� 0 �����̕�����ɂȂ� )
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
;	Kazumi
;	���ˏ��F
;
; Revision History:
;	93/ 7/16 Initial: dosargv0.asm/master.lib 0.20
;	93/ 7/21 [M0.20] small:bugfix

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GET_ARGV0	; dos_get_argv0() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; ����
	buf	= (RETSIZE+1)*2

	mov	AH,62h		; get psp
	int	21h

	mov	ES,BX
	mov	ES,ES:[2ch]
	mov	DI,0

	mov	CX,8000h	; ���ϐ��̈�� 32K�܂�
SEARCH:
	mov	AL,0
	repne	scasb		; �T���܂���
	cmp	AL,ES:[DI]	; 0
	jne	short SEARCH	; ������T��

	cmp	word ptr ES:[DI+1],1	; ��r���Ă邯�Ǐ�������͂�������

	lea	SI,[DI+1+2]
    s_ <push	DS>
	push	ES
	pop	DS

	_les	DI,[BP+buf]
    s_ <pop	ES>

	jne	short NAIJAN		; �� DOS�̂΁[����񂪈Ⴄ��łȂ���
					; AL�� 0 �������

	EVEN
COPY:	lodsb				; ������Ƃ��ᑬ�����
NAIJAN:	stosb
	or	AL,AL
	jnz	short COPY

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	DATASIZE*2
endfunc			; }

END
