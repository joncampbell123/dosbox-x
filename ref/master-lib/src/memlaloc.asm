; master library - MSDOS
;
; Description:
;	���C���������̏�ʂ���̊m��(�o�C�g���w��)
;
; Function/Procedures:
;	unsigned mem_lallocate( unsigned long bytesize ) ;
;
; Parameters:
;	unsigned bytesize	�m�ۂ���o�C�g��( 0�`0FFFFFh )
;
; Returns:
;	unsigned �m�ۂ��ꂽDOS�������u���b�N�̃Z�O�����g
;		 0 �Ȃ烁����������Ȃ��̂Ŏ��s�ł��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 3.0 or later
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
;	92/12/11 Initial
;	93/ 2/23 ����,�k��
;	93/ 3/ 4 bugfix

	.MODEL SMALL
	include func.inc
	.CODE

	EXTRN MEM_ALLOCATE:CALLMODEL

func MEM_LALLOCATE
	push	BP
	mov	BP,SP
	push	BX
	push	CX
	; ����
	sizehigh = (RETSIZE+2)*2
	sizelow	 = (RETSIZE+1)*2

	mov	AX,[BP+sizelow]
	mov	BX,[BP+sizehigh]
	add	AX,0fh		; 16byte�����͐؂�グ
	adc	BX,0
	cmp	BX,10h
	jnb	short FAULT
	and	AX,0fff0h
	or	AX,BX
	mov	CL,4
	ror	AX,CL
	push	AX
	call	MEM_ALLOCATE
RETURN:
	pop	CX
	pop	BX
	pop	BP
	ret	4
FAULT:
	xor	AX,AX
	jmp	short RETURN
endfunc

END
