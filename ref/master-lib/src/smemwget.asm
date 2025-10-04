; master library - MS-DOS
;
; Description:
;	�����������擾(64K�ȓ�)
;
; Functions:
;	unsigned smem_wget( unsigned bytesize ) ;
;
; Returns:
;	smem_wget
;		InsufficientMemory(cy = 1) �������s��
;		segment           (cy = 0) �m�ۂ���������
;
; Notes:
;	���W�X�^��AX�݂̂�j�󂵂܂��B
;	�܂������������蓖�Ă��Ă��Ȃ��ꍇ�Amem_assign_all()�����s���܂��B
;	smem_wget(0)�Ƃ��ČĂяo���ƁA���݂̖����ʒu�������܂��B
;
; Revision History:
;	93/ 3/20 Initial
;	93/ 9/14 [M0.21] �`���̌����� mem_TopHeap -> mem_TopSeg �ɏC��
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
	jc	short W_ERROR2
	; ���ɑ�����
endfunc
	; unsigned smem_wget( unsigned bytesize ) ;
func SMEM_WGET
	cmp	mem_TopSeg,0	; house keeping
	je	short ASSIGNALL

	push	BX
	mov	BX,SP

	; ����
	byte_size = (RETSIZE+1)*2

	mov	BX,SS:[BX+byte_size]
	add	BX,15		;�؂�グ
	rcr	BX,1		;paragraph size
	shr	BX,1
	shr	BX,1
	shr	BX,1

	mov	AX,mem_EndMark
	add	BX,AX
	jc	short W_ERROR	; more safety...

	cmp	mem_TopHeap,BX
	jc	short W_ERROR

	mov	mem_EndMark,BX
	pop	BX
	ret	2
	EVEN

W_ERROR:
	pop	BX
W_ERROR2:
	mov	AX,InsufficientMemory
	ret	2
endfunc

END
