; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̊O��������̏�������
;	���w��E��������
;
; Function/Procedures:
;	procedure gaiji_putnia( x,y,val,width_,attr : Word ) ;
;
; Parameters:
;	x	���[�̍��W ( 0 �` 79 )
;	y	��[�̍��W ( 0 �` 24 )
;	val	���l
;	width_	����( 1�` )
;	attr	�\������
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
;	width_ = 0 ���Ɖ������܂���B
;	�N���b�s���O���Ă܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/24 Initial: gjputs.asm
;	93/1/26 bugfix
;	93/8/17 Initial: gjputni.asm/master.lib 0.21
;	93/8/17 Initial: gjputnia.asm/master.lib 0.21

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func GAIJI_PUTNIA	; gaiji_putnia {
	push	BP
	mov	BP,SP

	; ����
	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	val	= (RETSIZE+3)*2
	width_	= (RETSIZE+2)*2
	attr	= (RETSIZE+1)*2

	mov	CX,[BP+width_]
	jcxz	short IGNORE

	push	SI
	push	DI

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

	mov	SI,[BP+val]

	mov	AX,[BP+attr]
	mov	BX,CX	; cx backup
	shl	CX,1
	add	DI,2000h
	rep	stosw	; �����h��ׂ�
	sub	DI,2002h
	mov	CX,BX	; cx restore

	mov	BX,10
	STD

	EVEN
SLOOP:
	xor	DX,DX
	mov	AX,SI
	div	BX
	mov	SI,AX

	mov	AH,DL
	add	AH,30h+80h
	mov	AL,56h
	stosw
	and	AH,7fh
	stosw

	loop	short SLOOP

	CLD

	pop	DI
	pop	SI

IGNORE:
	pop	BP
	ret	5*2
endfunc			; }

END
