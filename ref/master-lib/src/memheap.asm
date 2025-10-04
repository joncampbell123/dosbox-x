; master library
;
; Description:
;	�q�[�v�^�������}�l�[�W��
;
; Functions:
;	unsigned hmem_allocbyte( unsigned bytesize ) ;
;	unsigned hmem_alloc( unsigned parasize ) ;
;	void hmem_free( unsigned memseg ) ;
;
; Returns:
;	unsigned hmem_alloc:	 (cy=0) �m�ۂ����Z�O�����g
;				0(cy=1) �Ǘ��������s��
;
; Notes:
;	hmem_alloc ���s��́Amem_AllocID �� 0�ɂȂ�܂��B
;	hmem_alloc(0)�Ƃ��ČĂяo���ƁA�������s����Ԃ��܂��B
;
;	�q�[�v�̍\��
;		�Ǘ����: 16bytes
;			Using	2bytes	�g�p���Ȃ�1, ���g�p�Ȃ� 0
;			NextSeg	2bytes	���̃������u���b�N�̐擪
;					�����Ȃ� mem_OutSeg�Ɠ����l
;			ID	2bytes	�p�rID(�m�ێ���mem_AllocID�̒l��]��)
;		�����p���O���t���f�[�^��
;
; Assembly Language Note:
;	AX�ȊO�̑S�Ẵ��W�X�^��ۑ����܂��B
;
; Running Target:
;	MS-DOS
;
; Author:
;	���ˏ��F
;
; Rebision History:
;	93/ 3/20 Initial
;	93/ 5/ 3 bugfix: hmem_allocbyte(2�{�̊m�ۂ�����Ă���(^^;)
;	93/11/ 7 [M0.21] a=hmem_alloc(x); b=hmem_alloc(y);
;			hmem_free(a); hmem_free(b); �Ƃ����Ƃ���
;			mem_TopHeap���ُ�ɂȂ�o�O�C��(hmem_free��bug)
;			(������hole�ł��̒��O�ŗB��̃u���b�N���J�������Ƃ�)
;	93/11/ 7 [M0.21] hmem_free���Ahole���A�ڂ����Ƃ��ɐڑ����镔����
;			������������(;_;)
;	93/12/27 [M0.22] hmem_free�̘A�ڐڑ������S�ł͂Ȃ�����
;	93/12/29 [M0.22] hmem_free�ɓn�����l�̌������������ꂽ
;			(�Œ�l��菬�����Ƃ��ƁAusing flag��1�łȂ��Ƃ���
;			 ��������return����悤�ɂ���)
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/19 [M0.22k] �m�ۃT�C�Y��(mem_OutSeg-mem_EndMark)�ȏ�Ȃ�
;			��Ɏ��s��Ԃ��悤�ɂ����B
;	95/ 3/21 [M0.22k] BUGFIX hmem_free() �擪�̃u���b�N���J�������Ƃ���
;			���オ�t���[�u���b�N�ŁA���̎��̃t���[�u���b�N���ŏI
;			�u���b�N�̂Ƃ��ɁA���̍ŏI�t���[�u���b�N����������
;			mem_FirstHole��0�ɂ��Ă����B
;			�@���̂��߁A���ɖ�������2�Ԗڂ̃u���b�N���J�����Ă�
;			�ŏ��̋󂫂�������Ǝv���ĘA�ڍ�Ƃ��s�킸�A
;			������"�Y�ꋎ��ꂽ"�t���[�u���b�N�ƘA�ڂ��Ȃ������B
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD	; "Hole": �t���[�u���b�N�̂���
	EXTRN mem_EndMark:WORD
	EXTRN mem_AllocID:WORD

	.CODE

	EXTRN	MEM_ASSIGN_ALL:CALLMODEL

MEMHEAD STRUC
using	dw	?
nextseg	dw	?
mem_id	dw	?
MEMHEAD	ENDS

func HMEM_ALLOCBYTE	; hmem_allocbyte() {
	push	BX
	mov	BX,SP
	;
	bytesize = (RETSIZE+1)*2
	mov	BX,SS:[BX+bytesize]
	add	BX,15
	rcr	BX,1
	shr	BX,1
	shr	BX,1
	shr	BX,1
	jmp	short hmem_allocb
endfunc			; }

func HMEM_ALLOC		; hmem_alloc() {
	push	BX
	mov	BX,SP
	;
	parasize = (RETSIZE+1)*2

	mov	BX,SS:[BX+parasize]
hmem_allocb:
	cmp	mem_TopSeg,0	; house keeping
	jne	short A_S
	call	MEM_ASSIGN_ALL
A_S:
	push	CX
	push	ES

	test	BX,BX
	jz	short NO_MEMORY	; house keeping
	mov	AX,mem_OutSeg
	sub	AX,mem_EndMark
	cmp	BX,AX
	jae	short NO_MEMORY	; house keeping (add 95/3/19)

	inc	BX

	mov	AX,mem_FirstHole
	test	AX,AX
	jz	short ALLOC_CENTER	; ���S������̂�

	; �t���[�u���b�N����T��
SEARCH_HOLE_S:
	mov	CX,mem_OutSeg
SEARCH_HOLE:
	mov	ES,AX
	mov	AX,ES:[0].nextseg
	cmp	ES:[0].using,0
	jne	short SEARCH_HOLE_E
	mov	CX,ES
	add	CX,BX
	jc	short SEARCH_HOLE_E0	; 95/3/22
	cmp	CX,AX	; now+size <= next
	jbe	short FOUND_HOLE
SEARCH_HOLE_E0:
	mov	CX,mem_OutSeg
SEARCH_HOLE_E:
	cmp	AX,CX
	jne	short SEARCH_HOLE
	; �t���[�u���b�N�ɂ͂߂ڂ������̂͂Ȃ�����

ALLOC_CENTER:
	; ���S����m�ۂ���̂�
	mov	AX,mem_TopHeap
	mov	CX,AX
	sub	AX,BX
	jc	short NO_MEMORY		; 95/3/22
	cmp	AX,mem_EndMark
	jb	short NO_MEMORY
	mov	mem_TopHeap,AX
	mov	ES,AX
	mov	ES:[0].nextseg,CX
	mov	ES:[0].using,1
	mov	BX,AX
	jmp	short RETURN

NO_MEMORY:			; ����ȂƂ���
	mov	AX,0
	mov	mem_AllocID,AX
	stc
	pop	ES
	pop	CX
	pop	BX
	ret	2

	; ES=now
	; AX=next
	; CX=now+size
FOUND_HOLE:
	; �����t���[�u���b�N�����������̂�
	; �O�̕K�v����������؂���̂�
	sub	AX,CX
	cmp	AX,1	; �V�������� 0�`1�p���̑傫�������Ȃ��Ȃ猊���L����
	jbe	short JUST_FIT
	add	AX,CX
	mov	ES:[0].using,1
	mov	ES:[0].nextseg,CX
	mov	BX,ES
	mov	ES,CX
	mov	ES:[0].nextseg,AX
	mov	ES:[0].using,0
	cmp	BX,mem_FirstHole
	jne	short RETURN
	mov	mem_FirstHole,CX ; ���̂��擪�t���[�u���b�N��������X�V��
	jmp	short RETURN

	; ���傤�ǂ����z�Ńt���[�u���b�N�����������̂�
JUST_FIT:
	mov	ES:[0].using,1
	mov	BX,ES
	cmp	BX,mem_FirstHole
	jne	short RETURN
	; ���Ԃ����̂��擪�t���[�u���b�N�������̂Ȃ猟����
	mov	AX,mem_OutSeg
	mov	CX,BX
	push	BX
SEARCH_NEXT_HOLE:
	les	CX,ES:[0]	; CX=using, ES=nextseg
	jcxz	short FOUND_NEXT_HOLE
	mov	BX,ES
	cmp	BX,AX
	jb	short SEARCH_NEXT_HOLE
	xor	BX,BX
FOUND_NEXT_HOLE:
	mov	mem_FirstHole,BX
	pop	BX
	; jmp	short RETURN

 ; in: BX = �m�ۂł����������̊Ǘ��u���b�N��segment
RETURN:	mov	ES,BX
	mov	AX,0
	xchg	AX,mem_AllocID
	mov	ES:[0].mem_id,AX
	lea	AX,[BX+1]
	clc
NO_THANKYOU:				; hmem_free�̃G���[�͂����ɂ���(��)
	pop	ES
	pop	CX
	pop	BX
	ret	2
endfunc			; }


func HMEM_FREE		; hmem_free() {
	push	BX
	mov	BX,SP
	push	CX
	push	ES
	;
	memseg = (RETSIZE+1)*2
	mov	BX,SS:[BX+memseg]
	dec	BX
	mov	ES,BX
	cmp	BX,mem_TopHeap
	je	short EXPAND_CENTER	; �擪�̃u���b�N�Ȃ��p������
	jb	short NO_THANKYOU	; mem_TopHeap��菬�����Ȃ疳��

	; �擪�ł͂Ȃ��u���b�N�̊J������
	xor	BX,BX
	cmp	ES:[BX].using,1
	jne	short NO_THANKYOU	; using��1�łȂ���Ζ���
	mov	ES:[BX].using,BX	; using <- 0
	mov	CX,mem_FirstHole
	mov	AX,ES
	mov	mem_FirstHole,AX
	jcxz	short FREE_RETURN	; �����Ȃ������̂Ȃ炷��OK��
	cmp	AX,CX
	jb	short CONNECT_START
	mov	AX,CX		; �ȑO�̍ŏ��̌��ƌ��݈ʒu�̂ǂ��炩�Ⴂ����
	mov	mem_FirstHole,AX	; �V�����ŏ��̌��ɁB
CONNECT_START:

	mov	CX,AX
	mov	AX,ES:[BX].nextseg	; �I���n�_�́A�ړI�u���b�N�̎�������

	cmp	AX,mem_OutSeg
	jne	short NO_TAIL
	mov	AX,ES			; ������������A�I���n�_���ЂƂO��
NO_TAIL:
	; �E�A�ڂ��Ă��܂����t���[�u���b�N��ڑ�������
	push	DS
CONNECT_FREE:
	; �󂫂�T�����[�v
	mov	DS,CX			; DS<-CX
	mov	CX,[BX].nextseg		; CX=next seg
	cmp	CX,AX
	ja	short CONNECT_SKIP_OVER	; nextseg���ŏIseg���傫����ΏI���
	cmp	[BX].using,BX
	jne	short CONNECT_FREE	; �t���[�u���b�N�ɂȂ�܂Ői�߂�
	; �A�������󂫂��q�����[�v
CONNECT_FREE2:
	mov	ES,CX			; ES<-CX(=next seg)
	cmp	ES:[BX].using,BX	; �A�ڂ����t���[�܂Ői�߂�
	jne	short CONNECT_FREE
	; DS��ES�̃t���[�u���b�N���A�ڂ��Ă���
	mov	CX,ES:[BX].nextseg	; CX=�V����next seg
	mov	[BX].nextseg,CX		; �ڑ�
	cmp	CX,AX
	jbe	short CONNECT_FREE2	; CX���ŏIseg�ȓ��Ȃ�܂���������
CONNECT_SKIP_OVER:
	pop	DS
	jmp	short FREE_RETURN

	EVEN
EXPAND_CENTER:	; �擪�u���b�N�̊J��
	xor	BX,BX
	mov	AX,ES:[BX].nextseg
	mov	mem_TopHeap,AX
	cmp	AX,mem_OutSeg
	je	short FREE_RETURN	; ������������I��
	mov	ES,AX
	cmp	ES:[BX].using,BX
	jne	short FREE_RETURN	; ���̃u���b�N���g�p���Ȃ�I��

	; ������(ES:)���t���[�u���b�N�Ȃ̂ł������l�߂�B
	mov	AX,ES:[BX].nextseg
	mov	mem_TopHeap,AX
	; �Ȍ�̍ŏ��̃t���[�u���b�N���������A���̏ꏊ��mem_FirstHole�ɓ����
	mov	CX,mem_OutSeg
	cmp	AX,CX
	je	short FREE_LOST_HOLE	; ���ꂪ�ŏI�u���b�N�Ȃ���
	jmp	short X_SEARCH_HOLE
	EVEN
X_SEARCH_NEXT_HOLE:
	mov	AX,ES:[BX].nextseg
	cmp	AX,CX
	je	short FREE_LOST_HOLE
X_SEARCH_HOLE:
	mov	ES,AX
	cmp	ES:[BX].using,BX
	jne	short X_SEARCH_NEXT_HOLE
	mov	BX,ES
FREE_LOST_HOLE:
	mov	mem_FirstHole,BX

FREE_RETURN:
	clc
	pop	ES
	pop	CX
	pop	BX
	ret	2
endfunc			; }

END
