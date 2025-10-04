; master library - BGM
;
; Description:
;	EFS�t�@�C����ǂݍ���
;
; Function/Procedures:
;	int bgm_read_sdata(const char *fname);
;
; Parameters:
;	fname		�t�@�C���l�[��
;
; Returns:
;	BGM_COMPLETE	����I��
;	BGM_FILE_ERR	�t�@�C�����Ȃ�
;	BGM_FORMAT_ERR	�t�H�[�}�b�g���ُ�
;	BGM_OVERFLOW	������������Ȃ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	femy(��  ����)		: �I���W�i���EC�����
;	steelman(���  �T�i)	: �A�Z���u�������
;
; Revision History:
;	93/12/19 Initial: b_r_sdat.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc
	EXTRN	DOS_ROPEN:CALLMODEL
	EXTRN	DOS_SEEK:CALLMODEL
	EXTRN	DOS_READ:CALLMODEL
	EXTRN	DOS_CLOSE:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

	.DATA
	EXTRN	glb:WORD	; SGLB
	EXTRN	esound:WORD	; SESOUND

	.CODE

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	(DATASIZE*2)
	EVEN
endm

func BGM_READ_SDATA
	enter	4,0
	push	DI
	push	SI

	fname	= (RETSIZE+1)*2

	fbuf_seg= -2
	senum	= -4

	; �w�肵���t�@�C�����Ȃ���΃t�@�C���G���[
	_push	[BP+fname+2]
	push	[BP+fname]
	call	DOS_ROPEN
	mov	SI,AX
	cmp	SI,FileNotFound
	jne	short OPENED
	mov	AX,BGM_FILE_ERR
	jmp	short RETURN
NOMEM:	
	mov	AX,InsufficientMemory
RETURN:
	MRETURN

OPENED:
	push	AX
	push	0
	push	0
	push	2
	call	DOS_SEEK		; �Ō�ɂ���
	mov	DI,AX			; �����̉���16bit�����݂�
					; 64K�I�[�o�[�̓���͗\���s�\
	push	SI
	push	0
	push	0
	push	0
	call	DOS_SEEK		; �߂�
	push	DI
	call	SMEM_WGET
	jc	short NOMEM
	mov	[BP+fbuf_seg],AX
	push	SI
	push	AX
	push	CX
	push	DI
	call	DOS_READ
	mov	ES,[BP+fbuf_seg]
	mov	BX,AX
	mov	byte ptr ES:[BX],255	; �I����0ffh������
	push	SI
	call	DOS_CLOSE

	CLD

	;�f�[�^�Z�b�g
	mov	AX,glb.snum
	mov	[BP+senum],AX

	mov	BX,AX
	shl	BX,3			; type SESOUND
;	mov	AX,word ptr esound[BX].sbuf
;	mov	word ptr esound[BX].sptr,AX
	mov	word ptr esound[BX].sptr,0	; offset��0�̂͂�
	mov	AX,word ptr esound[BX].sbuf+2
	mov	word ptr esound[BX].sptr+2,AX

	xor	DI,DI		; dcnt
	xor	SI,SI
LOOP_TOP:
	mov	DS,[BP+fbuf_seg]
	ASSUME DS: NOTHING
TIL_NUMBER:
	lodsb
	cmp	AL,';'
	jne	short NO_COMMENT
COMMENT_SKIP:
	lodsb
	cmp	AL,0ffh
	je	short NUMBER_FOUND
	cmp	AL,0ah			; ';'����������s�܂Ŕ�΂�
	jne	short COMMENT_SKIP

NO_COMMENT:
	cmp	AL,'0'
	jb	short L01
	cmp	AL,'9'
	jbe	short NUMBER_FOUND
L01:
	cmp	AL,0ffh
	jne	short TIL_NUMBER
NUMBER_FOUND:
	xor	CX,CX		; dat = 0;

	; ����������
	cmp	AL,'0'
	jb	short NUMBER_END
WHILE_NUMBER:
	cmp	AL,'9'
	ja	short NUMBER_END
	cmp	AL,0ffh
	je	short NUMBER_END

	sub	AL,'0'
	mov	DL,AL
	mov	DH,0

	mov	AX,CX		; CX = CX * 10
	shl	AX,2
	add	AX,CX
	add	AX,AX
	add	AX,DX		; .... + (c - '0') ;
	mov	CX,AX
	lodsb
	cmp	AL,'0'
	jae	short WHILE_NUMBER
NUMBER_END:

	; �������I�����
	; CX�ɐ��l��������
	push	seg DGROUP
	pop	DS
	ASSUME DS: DGROUP

	cmp	AL,0ffh
	je	short LOOP_BREAK

	mov	BX,glb.snum
	shl	BX,3
	mov	DX,BX
	les	BX,esound[BX].sptr
	mov	ES:[BX],CX			; *esound[glb.senum].sptr = data
	mov	BX,DX
	add	word ptr esound[BX].sptr,2	; esound[glb.senum].sptr++

	or	CX,CX		; dat = SEND?
	je	short DAT_IS_SEND
	inc	DI		; dcnt
	cmp	DI,SBUFMAX
	jne	short NOT_SBUFMAX
DAT_IS_SEND:
	mov	BX,glb.snum
	shl	BX,3
	les	BX,esound[BX].sptr
	xor	DI,DI		; dcnt = 0;
	mov	ES:[BX],DI	; *esound[glb.senum].sptr = SEND
	inc	glb.snum
	cmp	glb.snum,SMAX
	je	short LOOP_BREAK	; break

	; esound[glb.senum].sptr = esound[glb.senum].sbuf ;
	mov	BX,glb.snum
	shl	BX,3
;	mov	CX,word ptr esound[BX].sbuf
;	mov	word ptr esound[BX].sptr,CX
	mov	word ptr esound[BX].sptr,0	; offset��0�̂͂�
	mov	CX,word ptr esound[BX].sbuf+2
	mov	word ptr esound[BX].sptr+2,CX
NOT_SBUFMAX:
	cmp	AL,0ffh
	je	short LOOP_BREAK
	jmp	LOOP_TOP
LOOP_BREAK:

	mov	AX,glb.snum
	cmp	[BP+senum],AX
	mov	AX,BGM_FORMAT_ERR
	je	short FORMERR		; ��������Ȃ������Ȃ玸�s
	xor	AX,AX		; BGM_COMPLETE
FORMERR:
	push	[BP+fbuf_seg]
	call	SMEM_RELEASE
	MRETURN
endfunc
END
