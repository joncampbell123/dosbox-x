; master library - MS-DOS
;
; Description:
;	�����������擾(64K�ȏ�)
;
; Functions:
;	unsigned smem_lget( unsigned long bytesize ) ;
;
; Returns:
;	InsufficientMemory(cy = 1) �������s��
;	segment           (cy = 0) �m�ۂ���������
;
; Notes:
;	���W�X�^��AX�݂̂�j�󂵂܂��B
;	�܂������������蓖�Ă��Ă��Ȃ��ꍇ�Amem_assign_all()�����s���܂��B
;	smem_lget(0)�Ƃ��ČĂяo���ƁA���݂̖����ʒu�������܂��B
;
; Revision History:
;	93/ 3/20 Initial
;	93/ 9/14 [M0.21] �`���̌����� mem_TopHeap -> mem_TopSeg �ɏC��
;	93/12/15 [M0.22] (x_x)bugfix, �����̏�ʃ��[�h�������_���ɂȂ��Ă�
;	95/ 3/23 [M0.22k] BUGFIX assign�m�ۂ���ĂȂ��Ƃ��Ɂc
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_EndMark:WORD

	.CODE
	EXTRN MEM_ASSIGN_ALL:CALLMODEL

retfunc ASSIGNALL
	call	MEM_ASSIGN_ALL
	jc	short L_ERROR2
	; ���ɑ�����
endfunc
	; unsigned smem_lget( unsigned long bytesize ) ;
func SMEM_LGET
	cmp	mem_TopSeg,0	; house keeping
	je	short ASSIGNALL

	push	BX
	mov	BX,SP

	; ����
	byte_size = (RETSIZE+1)*2

	mov	AX,SS:[BX+byte_size+2]
	mov	BX,SS:[BX+byte_size]
	add	BX,15		;�؂�グ
	adc	AX,0
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	jnz	short L_ERROR
	rcr	BX,1

	mov	AX,mem_EndMark
	add	BX,AX
	jc	short L_ERROR

	cmp	mem_TopHeap,BX
	jc	short L_ERROR

	mov	mem_EndMark,BX
	pop	BX
	ret	4
	EVEN

L_ERROR:
	pop	BX
L_ERROR2:
	stc
	mov	AX,InsufficientMemory
	ret	4
endfunc

END
