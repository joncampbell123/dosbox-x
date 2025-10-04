; master library - MS-DOS
;
; Description:
;	�������}�l�[�W���̏����ݒ�
;
; Functions/Procedures:
;	void mem_assign( unsigned top_seg, unsigned parasize ) ;
;	void mem_assign_all( void ) ;
;
; Noters:
;	���W�X�^�� AX�̂ݔj�󂵂܂��B
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/20 Initial
;	93/11/24 [M0.21] bugfix, ���������Ȃ��Ƃ��Ɂc
;       95/ 3/ 3 [M0.22k] mem_Reserve�ǉ�

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD
	EXTRN mem_EndMark:WORD
	EXTRN mem_MyOwn:WORD
	EXTRN mem_Reserve:WORD

	.CODE

	; void mem_assign( unsigned top_seg, unsigned parasize ) ;
	; �j��: AX�̂�
func MEM_ASSIGN
	push	BP
	mov	BP,SP
	; ����
	top_seg  = (RETSIZE+2)*2
	parasize = (RETSIZE+1)*2

	mov	AX,[BP+top_seg]
	mov	mem_TopSeg,AX
	mov	mem_EndMark,AX
	add	AX,[BP+parasize]
	mov	mem_OutSeg,AX
	mov	mem_TopHeap,AX
	mov	mem_FirstHole,0
	mov	mem_MyOwn,0
	clc

	pop	BP
	ret	4
endfunc

	; void mem_assign_all( void ) ;
	; �j��: AX�̂�
	; �������s���Ȃ� cy=1
func MEM_ASSIGN_ALL
	push	BX
	mov	BX,-1
	mov	AH,48h		; �ő�T�C�Y�𓾂�
	int	21h
	mov     AX,mem_Reserve
	cmp     BX,AX
	jbe     short ALL_ALLOC
	sub     BX,AX
ALL_ALLOC:
	mov	AH,48h		; ���̃T�C�Y�Ŋm�ۂ���
	int	21h
	jc	short NOMEM
	push	AX
	push	AX
	push	BX
	call	MEM_ASSIGN
	mov	mem_MyOwn,1
	pop	AX
NOMEM:
	pop	BX
	ret
endfunc

END
