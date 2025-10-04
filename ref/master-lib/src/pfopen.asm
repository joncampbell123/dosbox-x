; master library - (pf.lib)
;
; Description:
;	�t�@�C���̃I�[�v��
;
; Functions/Procedures:
;	pf_t pfopen(const char *parfile,const char far *file);
;
; Parameters:
;	parfile		par�t�@�C��
;	file		�I�[�v���������t�@�C����
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 186
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
;	iR
;	���ˏ��F
;
; Revision History:
; PFOPEN.ASM       5,703 94-09-17   23:22
;	95/ 1/10 Initial: pfopen.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 5/22 [M0.23] small/medium�Ō듮�삵�Ă����̂�"file"��far��

	.186
	.model SMALL
	include func.inc
	include pf.inc
	include super.inc

pfHeader	struc
	hd_type	db	?		; unsigned char type ���k�^�C�v
	hd_chk	db	?		; unsigned char check �`�F�b�N�o�C�g
	hd_aux	db	?		; unsigned char aux �⏕�t���O
	hd_fnm	db	13 dup(?)	; char filename[13] �t�@�C����
	hd_psz	dd	?		; long packsize �i�[�T�C�Y
	hd_osz	dd	?		; long orgsize �I���W�i���T�C�Y
	hd_time	dd	?		; long stamp �t�@�C���^�C���X�^���v
	hd_rsv	db	4 dup(?)	; char reserve[4] �\��
pfHeader	ends


; ParHeader �� type �̒l
	PF_NONE	equ	0	; �����k
	PF_LEN	equ	1	; ���������O�X

; pfHeader �� aux �̃r�b�g������
	PF_BODY_ENCRYPTED	equ	00000001b	; �t�@�C���{�̂��Í���
	PF_FNAME_INVISIBLE	equ	00000010b	; �t�@�C�������s��
	PF_AUX_AVAILABLE	equ	00000011b	; �L���ȃr�b�g

exeHeader	struc
	eh_M	db	?
	eh_Z	db	?
	eh_mod	dw	?
	eh_page	dw	?
exeHeader	ends

	.DATA
	EXTRN	pferrno:BYTE
	EXTRN	pfkey:BYTE
	EXTRN	mem_AllocID:WORD		; mem.asm

ext_exe	db	'.exe',0

	.CODE
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL
	EXTRN	BOPENR:CALLMODEL
	EXTRN	BCLOSER:CALLMODEL
	EXTRN	BREAD:CALLMODEL
	EXTRN	BSEEK_:CALLMODEL
	EXTRN	BSEEK:CALLMODEL

	EXTRN	PFGETX0:NEAR
	EXTRN	PFGETX1:NEAR
	EXTRN	PFGETC1:NEAR

func PFOPEN	; pfopen() {
	enter	size pfHeader,0
	push	SI
	push	DI

	;arg	parfile:dataptr,file:dataptr
	parfile	= (RETSIZE+1+2)*2
	file	= (RETSIZE+1)*2

	;local	header:pfHeader
	header = -(size pfHeader)

	; PFILE�p�o�b�t�@�擾
	mov	mem_AllocID,MEMID_pfile
	push	size PFILE
	_call	HMEM_ALLOCBYTE
	jc	PFOPEN_nomem
	mov	SI,AX			; PFILE�\���̂̃Z�O�����g

	; par�t�@�C�����I�[�v��
	_push	[BP+parfile+2]
	push	[BP+parfile]
	_call	BOPENR

	or	AX,AX
	jz	PFOPEN_free
	mov	ES,SI
	mov	ES:[pf_bf],AX

	; �^�[�Q�b�g�t�@�C���܂ŃV�[�N
	lea	DI,[BP+header]
	;call	SEEK Pascal,DI,SI,[BP+parfile],[BP+file]
	push	DI		; _ss * header
	push	SI		; pf
	_push	[BP+parfile+2]
	push	[BP+parfile]
	push	[BP+file+2]
	push	[BP+file]
	call	near ptr SEEK

	or	AX,AX
	jnz	short PFOPEN_close

	; �Í����̗L���ɑΉ����ē���1byte�Ǐo���֐����Z�b�g����
	mov	AX,offset PFGETX0
	test	SS:[DI.hd_aux],PF_BODY_ENCRYPTED
	jz	short PFOPEN_1
	mov	AX,offset PFGETX1
PFOPEN_1:
	mov	word ptr ES:[pf_getx],AX
	;	pf�n�֐��͂��ׂē����Z�O�����g�ł���

	; �������p�L�[
	mov	AL,pfkey
	mov	ES:[pf_key],AL

	mov	AL,SS:[DI.hd_type]
	cmp	AL,PF_NONE
	je	short PFOPEN_pf_none
	cmp	AL,PF_LEN
	je	short PFOPEN_pf_len
	mov	AX,PFEUNKNOWN
	jmp	short PFOPEN_close

PFOPEN_pf_none:
	mov	AX,word ptr ES:[pf_getx]
	mov	word ptr ES:[pf_getc],AX
	jmp	short PFOPEN_return

PFOPEN_pf_len:
	mov	word ptr ES:[pf_getc],offset PFGETC1
	mov	ES:[pf_cnt],0
	mov	ES:[pf_ch],EOF

PFOPEN_return:
	mov	AX,word ptr SS:[DI.hd_psz]
	mov	DX,word ptr SS:[DI.hd_psz + 2]
	mov	word ptr ES:[pf_size],AX
	mov	word ptr ES:[pf_size + 2],DX

	mov	AX,word ptr SS:[DI.hd_osz]
	mov	DX,word ptr SS:[DI.hd_osz + 2]
	mov	word ptr ES:[pf_osize],AX
	mov	word ptr ES:[pf_osize + 2],DX

	xor	AX,AX
	mov	word ptr ES:[pf_read],AX
	mov	word ptr ES:[pf_read + 2],AX
	mov	word ptr ES:[pf_loc],AX
	mov	word ptr ES:[pf_loc + 2],AX

	mov	AX,SI
	jmp	short PFOPEN_q

PFOPEN_close:
	mov	word ptr pferrno,AX

	push	ES:[pf_bf]
	_call	BCLOSER
PFOPEN_free:
	push	SI
	_call	HMEM_FREE
	jmp	short PFOPEN_error
PFOPEN_nomem:
	mov	pferrno,PFENOMEM
PFOPEN_error:
	xor	AX,AX

PFOPEN_q:
	pop	DI
	pop	SI
	leave
	ret	(2+DATASIZE)*2
endfunc	; }


;-----------------------------------------------------------------------------
; seek(pfHeader _ss *header, pf_t pf, const char MASTER_PTR *parfile,const char MASTER_PTR *file)
;-----------------------------------------------------------------------------
SEEK	proc near	; {
	enter	size exeHeader,0
	push	SI
	push	DI
	push	ES

	;arg	header:dataptr,pf:word, parfile:dataptr,file:dataptr
	header	= (1+2+2+DATASIZE)*2
	pf	= (1+1+2+DATASIZE)*2
	parfile	= (1+1+2)*2
	file	= (1+1)*2

	;local	exeh:exeHeader
	exeh = -(size exeHeader)

	mov	SI,[BP+pf]		; PFILE�\���̂̃Z�O�����g

	mov	ES,SI
	xor	AX,AX
	mov	word ptr ES:[pf_home],AX
	mov	word ptr ES:[pf_home + 2],AX

	; [DX:]BX = strrchr(parfile,'.');
	_push	DS
	push	SI
	CLD
	xor	BX,BX
	_mov	DX,BX
	_lds	SI,[BP+parfile]
STRRCHR_LOOP:
	lodsb
	cmp	AL,0
	jz	short STRRCHR_DONE
	cmp	AL,'.'
	jne	short STRRCHR_LOOP
	lea	BX,[SI-1]
	_mov	DX,DS
	jmp	short STRRCHR_LOOP
STRRCHR_DONE:
	pop	SI
	_pop	DS
    l_ <mov	AX,DX>
    l_ <or	AX,BX>
    s_ <test	BX,BX>
	jz	short SEEK_seek

	push	DS
	push	offset DGROUP:ext_exe
	_push	DX
	push	BX
	call	near ptr STR_IEQ
	jz	short SEEK_seek

	; EXE�t�@�C��
	mov	ES,SI
	lea	DI,[BP+exeh]
	;call	BREAD Pascal,DI,size exeHeader,ES:[pf_bf]
	_push	SS
	push	DI
	push	size exeHeader
	push	ES:[pf_bf]
	_call	BREAD

	mov	BX,PFEILEXE
	cmp	AX,size exeHeader
	jne	SEEK_error
	mov	BX,10
	cmp	[DI.eh_M],'M'
	jne	SEEK_error
	mov	BX,11
	cmp	[DI.eh_Z],'Z'
	jne	SEEK_error

	mov	AX,[DI.eh_page]
	dec	AX
	mov	DX,512
	mul	DX
	add	AX,[DI.eh_mod]
	;adc	DX,0	; ����Ȃ�

	mov	ES,SI
	mov	word ptr ES:[pf_home],AX
	mov	word ptr ES:[pf_home + 2],DX

	push	ES:[pf_bf]
	push	DX
	push	AX
	push	0
	_call	BSEEK_

SEEK_seek:
	mov	DI,[BP+header]
SEEK_loop:
	; �w�b�_�����[�h
	mov	ES,SI
	;call	BREAD Pascal,DI,size pfHeader,ES:[pf_bf]
	_push	SS
	push	DI
	push	size pfHeader
	push	ES:[pf_bf]
	_call	BREAD

	cmp	AX,size pfHeader
	mov	BX,PFENOTFOUND
	jne	short SEEK_error

	mov	ES,SI
	add	word ptr ES:[pf_home],AX
	adc	word ptr ES:[pf_home + 2],0

	; �w�b�_�̐��������`�F�b�N
	mov	AL,SS:[DI.hd_type]
	xor	AL,SS:[DI.hd_chk]
	inc	AL
	mov	BX,PFEILPFILE
	jnz	short SEEK_error

	; �t�@�C�������s���̏ꍇ�A��r�̂��߂��Ƃɖ߂�
	test	SS:[DI.hd_aux],PF_FNAME_INVISIBLE
	jz	short SEEK_cmp
	lea	BX,[DI.hd_fnm]
SEEK_not:	cmp	byte ptr SS:[BX],0
	je	short SEEK_cmp
	not	byte ptr SS:[BX]
	inc	BX
	jmp	short SEEK_not

SEEK_cmp:
	lea	BX,[DI.hd_fnm]

	push	[BP+file+2]
	push	[BP+file]
	_push	SS
	push	BX
	call	near ptr STR_IEQ
	mov	AX,0
	jnz	short SEEK_return

	mov	ES,SI
	mov	AX,word ptr SS:[DI.hd_psz]
	mov	DX,word ptr SS:[DI.hd_psz + 2]
	add	word ptr ES:[pf_home],AX
	adc	word ptr ES:[pf_home + 2],DX

	push	ES:[pf_bf]
	push	DX
	push	AX
	_call	BSEEK

	jmp	short SEEK_loop

SEEK_error:
	mov	AX,BX

SEEK_return:
	pop	ES
	pop	DI
	pop	SI
	leave
	ret	(2+2+DATASIZE)*2
SEEK	endp		; }


;-----------------------------------------------------------------------------
;	int str_ieq(const char far *s1,const char MASTER_PTR *s2);
;		������s1��s2��啶���������𓯈ꎋ���Ĕ�r���A
;		equal�Ƃ�0�ȊO(zf=0)�Anot equal�̂Ƃ�0(zf=1)��Ԃ�
;-----------------------------------------------------------------------------
STR_IEQ	proc near	; {
	push	BP
	mov	BP,SP
	push	SI
	_push	DS

	;arg	s1:dataptr,s2:dataptr
	s1	= (1+1+DATASIZE)*2
	s2	= (1+1)*2

	CLD
	les	BX,[BP+s1]
	_lds	SI,[BP+s2]

	EVEN
STR_IEQ_loop:
	mov	AH,ES:[BX]
	inc	BX
	lodsb

	sub	AL,'a'
	cmp	AL,'z' - 'a'
	ja	short STR_IEQ_1
	sub	AL,'a' - 'A'	; toupper
STR_IEQ_1:
	sub	AH,'a'
	cmp	AH,'z' - 'a'
	ja	short STR_IEQ_2
	sub	AH,'a' - 'A'	; toupper
STR_IEQ_2:
	cmp	AH,AL
	jne	short STR_IEQ_noteq

	add	AL,'a'
	jnz	short STR_IEQ_loop
	jmp	short STR_IEQ_ret

STR_IEQ_noteq:
	xor	AX,AX
STR_IEQ_ret:
	test	AX,AX
	_pop	DS
	pop	SI
	pop	BP
	ret	(DATASIZE+2)*2
STR_IEQ	endp		; }

END
