; master library - MS-DOS
;
; Description:
;	�[���f�B���N�g�����쐬����
;
; Function/Procedures:
;	int dos_makedir( const char * path ) ;
;
; Parameters:
;	path	�p�X��(�Ō�� :,\�܂���/���t���Ă���Ǝ��s���܂�)
;
; Returns:
;	0	���s(�h���C�u�������A�������ȕ���������Ȃ�)
;	1	����
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
;	���s�����ꍇ�A�r���܂Ńf�B���N�g�����쐬����Ă���ꍇ������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/14 Initial

	.MODEL SMALL
	include func.inc

	EXTRN	FILE_BASENAME:CALLMODEL

	.CODE

; ���ۂ̍쐬����
; IN: DS:DI = char * const path
; IO: SI=result( 1�ŏ��������邱�� )
makedir	PROC NEAR

	_push	DS
	push	DI
	call	FILE_BASENAME	; BX = file_basename(path) ;
	mov	BX,AX
	xor	AX,AX

	cmp	[BX],AL		; �h���C�u�w�� or root�Ȃ玸�s
	stc
	je	short MAKE_RESULT

	mov	DX,DI
	mov	AH,39h		; makedir
	int	21h		; ����Ă݂Ăł����琬��
	jnb	short MAKE_RET
	cmp	AX,5
	je	short MAKE_RET; ���݂��邩root�ɋ󂫂��Ȃ���ΐ���!!(��)

	xor	AX,AX
	push	[BX-1]		; �ʖڂȂ��������Ă݂�
	mov	[BX-1],AL	; 0
	push	BX
	call	makedir
	pop	BX
	pop	AX
	mov	[BX-1],AL

	test	SI,SI
	jz	short MAKE_RET

	mov	AH,39h		; makedir
	mov	DX,DI
	int	21h		; ��オ�ł����������x����Ă݂�
MAKE_RESULT:
	sbb	SI,SI
	inc	SI

MAKE_RET:
	ret
	EVEN
makedir	ENDP


func DOS_MAKEDIR
	mov	BX,SP
	push	DI
	_push	DS

	_lds	DI,SS:[BX+RETSIZE*2]	; DS:DI = path

	xor	AX,AX

	cmp	BYTE PTR [DI+1],':'
	jne	short NODRIVE

	mov	AL,[DI]
	and	AL,01fh		; make drive number( 0 = '@' )

NODRIVE:
	mov	DL,AL
	mov	AH,36h
	int	21h		; �h���C�u�̗L���������̂��߂Ɏc��e�ʂ��c
	inc	AX
	jz	short EXIT

	push	SI
	mov	SI,1
	call	makedir
	mov	AX,SI
	pop	SI

EXIT:
	_pop	DS
	pop	DI
	ret	DATASIZE*2
endfunc

END
