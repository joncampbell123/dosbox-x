; master library - heap
;
; Description:
;	heap�������̃T�C�Y�ύX
;
; Function/Procedures:
;	unsigned hmem_reallocbyte( unsigned oseg, unsigned bytesize ) ;
;	unsigned hmem_realloc( unsigned oseg, unsigned parasize ) ;
;
; Parameters:
;	oseg	  �]����hmem�������u���b�N
;	bytesize  �V�����傫��(�o�C�g�P��)
;	parasize  �V�����傫��(�p���O���t�P��)
;
; Returns:
;	�V�����Z�O�����g�ʒu
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Assembly Language Note:
;	AX�ȊO�̃��W�X�^��ۑ����܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 1/ 1 Initial: hmemreal.asm/master.lib 0.22
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/19 [M0.22k] �V�T�C�Y��(mem_OutSeg-mem_EndMark)�ȏ�Ȃ�
;			hmem_alloc�ɓn���ăG���[�ɂ�����悤�ɂ���
;	95/ 3/21 [M0.22k] BUGFIX ���e�]�ʂ����܂������Ă��Ȃ�����

	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD
	EXTRN mem_EndMark:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN HMEM_ALLOC:CALLMODEL
	EXTRN HMEM_FREE:CALLMODEL

MEMHEAD STRUC
using	dw	?
nextseg	dw	?
mem_id	dw	?
MEMHEAD	ENDS

	oseg = (RETSIZE+2)*2

func HMEM_REALLOCBYTE	; hmem_reallocbyte() {
	push	BX
	mov	BX,SP
	push	CX
	;
	bytesize = (RETSIZE+1)*2
	mov	CX,SS:[BX+oseg]
	mov	BX,SS:[BX+bytesize]
	add	BX,15
	rcr	BX,1
	shr	BX,1
	shr	BX,1
	shr	BX,1
	jmp	short hmem_reallocb
endfunc			; }

NEW_ALLOC proc CALLMODEL	; oseg=0�̂Ƃ��́A�V�K�m��
	push	BX
	call	HMEM_ALLOC
	pop	CX
	pop	BX
	ret	4
NEW_ALLOC endp

OLD_FREE proc CALLMODEL		; bytesize|parasize = 0�̂Ƃ��́A�J��
	push	BX
	call	HMEM_FREE
	xor	AX,AX
	mov	mem_AllocID,AX
	pop	CX
	pop	BX
	ret	4
OLD_FREE endp

func HMEM_REALLOC	; hmem_realloc() {
	push	BX
	mov	BX,SP
	push	CX
	;
	parasize = (RETSIZE+1)*2

	mov	CX,SS:[BX+oseg]
	mov	BX,SS:[BX+parasize]
hmem_reallocb:
	mov	AX,mem_OutSeg
	sub	AX,mem_EndMark
	cmp	BX,AX
	jae	short NEW_ALLOC	; �ł������� hmem_alloc �ɃG���[�ɂ�����
	jcxz	short NEW_ALLOC
	test	BX,BX
	jz	short OLD_FREE

	dec	CX
	push	ES
	push	SI
	xor	SI,SI		; 0

	inc	BX

	; house keeping
	cmp	CX,mem_TopHeap	; �Œ�q�[�v�����Ⴂ�Ȃ� zero return
	jb	short NO_GOOD
	cmp	CX,mem_OutSeg	; �ō��q�[�v�A�h���X��荂���Ȃ� zero return
	jae	short NO_GOOD
	mov	ES,CX
	cmp	ES:[SI].using,1	; using�t���O���u�g�p��(1)�v�łȂ���� zero ret
	jne	short NO_GOOD

	; ���Ė{��
	mov	AX,ES:[SI].mem_id	; ID��]�ʂ��Ă���
	mov	mem_AllocID,AX

	mov	AX,ES:[SI].nextseg ; AX=nextseg
	add	BX,CX		; BX= �v�������傫���ɂ����Ƃ��̎��ɂȂ�ʒu
	jc	short OTHER_PLACE	; carry�ł�Ȃ��Ίg��ő��̏ꏊ
	cmp	AX,BX
	jae	short SHRINK

	; �傫���Ȃ�Ƃ�
	cmp	AX,mem_OutSeg
	jae	short OTHER_PLACE	; �ŏI�u���b�N�Ȃ�ړ����邵���Ȃ�
	mov	ES,AX
	cmp	ES:[SI].using,SI
	jne	short OTHER_PLACE	; ���̃u���b�N���g�p���Ȃ��͂�ړ�

	; ���ɋ�Ԃ�����
	cmp	ES:[SI].nextseg,BX
	jb	short OTHER_PLACE	; ���̋�Ԃ𑫂��Ă�����Ȃ�?
	; �����ꍇ
	cmp	AX,mem_FirstHole
	jne	short CONNECT
	; FirstHole���Ԃ����߂ɁA�m�ۂ���̂�
	sub	AX,ES:[SI].nextseg
	not	AX			; (nextseg - freeseg) - 1
	push	AX
	call	HMEM_ALLOC
CONNECT:
	mov	AX,ES:[SI].nextseg
	mov	ES,CX
	mov	ES:[SI].nextseg,AX	; ��������
	cmp	AX,BX
SHRINK:
	je	short HOP_OK		; ��v������OK
IF 0
	; in: ES=CX=���݂�seg
	inc	BX
	cmp	AX,BX
	je	short HOP_OK		; �ړI�̖���+1=���ݖ����Ȃ�A��͂�OK
	dec	BX
ENDIF

	; �k�߂�
	mov	ES:[SI].nextseg,BX	; BX(���z�̎��ʒu)����������
	mov	ES,BX
	mov	ES:[SI].nextseg,AX	; ���̒n�_�̎��ʒu
	mov	ES:[SI].using,1		; �g�p���̃u���b�N��ɕ��f
	inc	BX			; (�����J�����邩��mem_AllocID�͕s�v)
	push	BX
	call	HMEM_FREE
HOP_OK:	jmp	short OK

NO_GOOD:			; ���s�̂Ƃ��� 0 ��Ԃ��̂͂���
	xor	AX,AX
	jmp	short RETURN

	; ���Ɍ��Ԃ��Ȃ�����A�ʂ̏ꏊ�ɍĊm�ۂ���
OTHER_PLACE:
	; CX=���݂̃������u���b�N�̈ʒu
	; BX=CX�ɗv�����ꂽ�V�����傫�������������̈ʒu
	; AX=���݂̎��̈ʒu( �����ɗ���Ȃ��� BX > AX )
	cmp	CX,mem_TopHeap
	jne	short RE_ALLOC
	; �擪�Ȃ�΁A����Ȃ��傫�������O�ɖ߂�Ηǂ�
	sub	BX,AX	; BX=����Ȃ���
	sub	CX,BX	; CX=���̕������߂����ʒu
	jb	short RE_ALLOC1		; 95/3/22
	cmp	CX,mem_EndMark
	jb	short RE_ALLOC1		; ����ς葫��Ȃ��ꍇ
	mov	ES,CX
	mov	ES:[SI].nextseg,AX
	mov	ES:[SI].using,1
	push	mem_AllocID
	pop	ES:[SI].mem_id
	mov	mem_TopHeap,CX
	add	BX,CX
	sub	AX,BX
	; CX=�V�����ʒu(�Ǘ��̈�̈ʒu)
	; BX=�Â��ʒu(�V)
	; AX=�Â��傫��(�g��̂Ƃ��̏���������A��ɐV�����傫����菬����)
	jmp	short TRANS
	EVEN

	; �^���Ɋg�債�悤�Ƃ��Ăł��Ȃ������Ƃ��̏���
	; �E�擪�̃u���b�N�Ȃ�O�Ɋg�傷��B
	; �E���ꂪ�ł��Ȃ��Ƃ��́A�ʂ̈ʒu�ɐV�K�m�ۂ���B
	; �E�ǂ���ɂ���A�V�����ʒu�ɈȑO�̓��e��]�ʂ���B

	
	; AX=���݂̎��̈ʒu
	; BX=����Ȃ���(����)
	; CX=���̕������߂����ʒu
RE_ALLOC1:
	add	CX,BX
	add	BX,AX

	; CX=���݂̃������u���b�N�̈ʒu
	; BX=CX�ɗv�����ꂽ�V�����傫�������������̈ʒu
	; AX=���݂̎��̈ʒu( �����ɗ���Ȃ��� BX > AX )

RE_ALLOC:
	sub	AX,CX		; AX=�Â��傫��
	sub	BX,CX		; BX=�V�����傫��
	xchg	AX,BX		; AX=�V�����傫��, BX=�Â��傫��
	xchg	BX,CX		; BX=�Â��ʒu, CX=�Â��傫��
	dec	AX
	push	AX
	call	HMEM_ALLOC
	jc	short NO_GOOD
	dec	AX
	xchg	AX,CX
	push	AX
	lea	AX,[BX+1]
	push	AX
	call	HMEM_FREE	; ��ɊJ�������Ⴄ
	pop	AX

	; �ȑO�̓��e��]�ʂ���B
	; CX=�V�����ʒu(�Ǘ��̈�̈ʒu)
	; BX=�Â��ʒu(�V)
	; AX=�Â��傫��(�g��̂Ƃ��̏���������A��ɐV�����傫����菬����)
TRANS:	push	CX
	push	DI
	push	DS
	CLD
TRANSLOOP:				; �蔲���̓]��(^^;
	inc	CX
	inc	BX
	mov	ES,CX
	mov	SI,0
	mov	DS,BX
	mov	DI,SI
	mov	CX,16/2
	rep	movsw
	mov	CX,ES
	dec	AX
	jnz	short TRANSLOOP
	pop	DS
	pop	DI
	pop	CX
OK:
	inc	CX
	mov	AX,CX
RETURN:
	mov	mem_AllocID,0
	pop	SI
	pop	ES
	pop	CX
	pop	BX
	ret	4
endfunc		; }

END
