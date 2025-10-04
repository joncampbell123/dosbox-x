; master library
;
; Description:
;	�q�[�v�������擾 long��
;
; Functions:
;	unsigned hmem_lallocate( unsigned long bytesize ) ;
;
; Returns:
;	unsigned 	 (cy=0) �m�ۂ����Z�O�����g
;			0(cy=1) �Ǘ��������s��
;
; Notes:
;	AX�ȊO�̑S�Ẵ��W�X�^��ۑ����܂��B
;	hmem_lallocate(0)�Ƃ��ČĂяo���ƁA�������s����Ԃ��܂��B
;
; Running Target:
;	MS-DOS
;
; Author:
;	���ˏ��F
;
; Rebision History:
;	93/ 3/31 Initial
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	HMEM_ALLOC:CALLMODEL

func HMEM_LALLOCATE	; hmem_lallocate() {
	push	BX
	mov	BX,SP
	;
	byteh = (RETSIZE+2)*2
	bytel = (RETSIZE+1)*2
	mov	AX,SS:[BX+byteh]
	mov	BX,SS:[BX+bytel]
	add	BX,15
	adc	AX,0

	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	jnz	short NOMEM
	rcr	BX,1

	push	BX
	call	HMEM_ALLOC
	pop	BX
	ret	4

NOMEM:
	mov	AX,0
	stc
	pop	BX
	ret	4
endfunc			; }

END
