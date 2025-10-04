; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̕����̏�������
;	������
;
; Function/Procedures:
;	void text_putca( unsigned x, unsigned y, unsigned chr, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	unsigned chr	����(ANK, JIS �܂��� SHIFT-JIS�R�[�h)
;	unsigned atrb	����
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�������Ȃ������R�[�h���^����ꂽ�ꍇ�A�������ȕ������\������܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/15 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE


func TEXT_PUTCA
	mov	DX,BP	; push BP
	mov	BP,SP

	mov	CX,DI	; push DI

	; ����
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	chr	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	mov	AX,[BP + y]	; �A�h���X�v�Z
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1		; DI = y * 10
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,[BP + x]
	shl	DI,1

	mov	AX,[BP + chr]
	mov	BX,[BP + atrb]

	mov	BP,DX	; pop BP

	or	AH,AH
	jz	short ANK_OR_RIGHT

	cmp	AH,80h	; 		81-9f e0-fd ?
	jb	short JIS
KANJI:		; SHIFT-JIS to JIS
		shl	AH,1
		cmp	AL,9fh
		jnb	short SKIP
			cmp	AL,80h
			adc	AX,0fedfh
SKIP:		sbb	AX,0dffeh		; -(20h+1),-2
		and	AX,07f7fh

JIS:		xchg	AH,AL
		sub	AL,20h
		mov	ES:[DI+2000h],BX	; ����
		stosw
		or	AL,80h
	ANK_OR_RIGHT:
	mov	ES:[DI+2000h],BX		; ����
	stosw

	mov	DI,CX	; pop	DI
	ret	8
endfunc

END
