; master library - BGM
;
; Description:
;	�������E��n��������
;
; Function/Procedures:
;	int bgm_init(int bufsiz);
;	void bgm_finish(void);
;
; Parameters:
;	bufsiz			1�p�[�g������̃o�b�t�@�T�C�Y
;
; Returns:
;	BGM_COMPLETE		����I��
;	BGM_OVERFLOW		������������Ȃ�
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
;	����
;
; Revision History:
;	93/12/19 Initial: b_inifin.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 4/11 [M0.23] AT�݊��@�Ή�
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;       95/ 4/ 1 [M0.22k] �������s�����̃������J��������C��
;	95/ 4/ 2 [M0.22k] BUGFIX EFS�������̊m�ۃT�C�Y��256�v�f�ɂ͑���Ȃ�����

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc
	EXTRN	GET_MACHINE:CALLMODEL           ; getmachi.asm
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL        ; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL             ; memheap.asm

	.DATA
	EXTRN	glb:WORD	;SGLB           ; b_data.asm
	EXTRN	part:WORD	;SPART          ; b_data.asm
	EXTRN	esound:WORD	;SESOUND        ; b_data.asm
	EXTRN	mem_AllocID:WORD		; mem.asm

	EXTRN	Machine_State:WORD              ; machine.asm

	.CODE
	EXTRN	_BGM_TIMER_INIT:CALLMODEL       ; b_timer.asm
	EXTRN	_BGM_TIMER_FINISH:CALLMODEL     ; b_timer.asm
	EXTRN	BGM_SET_TEMPO:CALLMODEL         ; b_s_temp.asm
	EXTRN	BGM_STOP_PLAY:CALLMODEL         ; b_sp_ply.asm
	EXTRN	BGM_STOP_SOUND:CALLMODEL        ; b_sp_snd.asm

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	2
	EVEN
endm

func BGM_INIT	; bgm_init() {
	push	BP
	mov	BP,SP
	sub	SP,2
	push	SI
	push	DI
	bsize	= (RETSIZE+1)*2
	bsize2	= -2

	call	GET_MACHINE

	cmp	glb.init,0
	je	short NOTINITIALIZED1
	xor	AX,AX			; BGM_COMPLETE
	MRETURN

NOTINITIALIZED1:
	;bsize2 = bufsiz;
	mov	DX,[BP+bsize]
	mov	[BP+bsize2],DX
	or	DX,DX
	jg	short SETBUFSIZE
	mov	word ptr [BP+bsize2],BUFMAX
SETBUFSIZE:
	mov	AX,[BP+bsize2]
	mov	glb.bufsiz,AX

	;�ȃo�b�t�@�m��
	mov	DI,offset part
ALLOCATEMBUF:
	;if ((part[cnt].mbuf = MK_FP(hmem_allocbyte(bsize2),0)) == NULL)
	mov	mem_AllocID,MEMID_bgm
	push	[BP+bsize2]
	call	HMEM_ALLOCBYTE
	mov	word ptr [DI].mbuf+2,AX
	mov	word ptr [DI].mbuf,0
	jnc	short MBUFALLOCATED
	mov	AX,BGM_OVERFLOW
	MRETURN
MBUFALLOCATED:
	add	DI,type SPART
	cmp	DI,offset part + (type SPART * PMAX)
	jne	short ALLOCATEMBUF

	;���ʉ��o�b�t�@�m��
	mov	DI,offset esound
ALLOCATESBUF:
	;if ((esound[cnt].sbuf = MK_FP(hmem_allocbyte(SBUFMAX * sizeof(uint) + 1),0)) == NULL)
	mov	mem_AllocID,MEMID_efs
	push	(SBUFMAX * 2)+1
	call	HMEM_ALLOCBYTE
	mov	word ptr [DI].sbuf+2,AX
	mov	word ptr [DI].sbuf,0
	jnc	short SBUFALLOCATED
	push	word ptr [DI].mbuf+2
	mov	word ptr [DI].mbuf+2,0
	call    HMEM_FREE
	mov	AX,BGM_OVERFLOW
	MRETURN
SBUFALLOCATED:
	add	DI,type SESOUND
	cmp	DI,offset esound + (type SESOUND * SMAX)
	jne	short ALLOCATESBUF

	;BGMOFF
	mov	glb.rflg,OFF
	;�o�^�Ȑ�
	mov	glb.mnum,0
	;�Z���N�g���Ȕԍ�
	mov	glb.mcnt,0
	;�p�[�g��
	mov	glb.pnum,PMAX
	;�p�[�g�J�E���^
	mov	glb.pcnt,0
	;�I���p�[�g��
	mov	glb.fin,0
	;���s�[�gON
	mov	glb.repsw,ON
	;�����J�E���^
	mov	glb.tcnt,0
	;�ȃo�b�t�@���� 
	mov	glb.buflast,0
	;�Ȃ��Ƃ̃e���|��� 

	CLD

	mov	CX,MMAX
	mov	DI,offset glb.mtp
	push	DS
	pop	ES
	mov	AX,DEFTEMPO
	rep 	stosw
	;���ʉ�OFF
	mov	glb.effect,OFF
	;�o�^���ʉ���
	mov	glb.snum,0
	;�Z���N�g�����ʉ��ԍ�
	mov	glb.scnt,0
	;���t����ON
	mov	glb.music,ON
	;���ʉ�����ON
	mov	glb.sound,ON
	;glb.clockbase = ((ulong)TVAL1ms * 120UL);
	;tempo120�̃^�C�}�J�E���g
	test	Machine_State,10h	; PC/AT
	jz	short PC98
	mov	AX,TVALATORG_RTC
	jmp	short SET_CLOCKBASE
PC98:
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[0501H],80h
	mov	AX,TVAL8ORG
	jnz	short CLOCK8MHZ
	mov	AX,TVAL10ORG
CLOCK8MHZ:
SET_CLOCKBASE:
	and	AX,0fffeh
	mov	DX,DEFTEMPO
	mul	DX
	mov	word ptr glb.clockbase+2,DX
	mov	word ptr glb.clockbase,AX

	push	DEFTEMPO
	call	BGM_SET_TEMPO

	;�o�b�t�@�N���A
	mov	BX,offset part.mbuf
	xor	DX,DX
	xor	AX,AX
BUFFERCLEARLOOP:
	les	DI,[BX]
	mov	CX,glb.bufsiz
	shr	CX,1
	rep	stosw
	adc	CX,CX
	rep	stosb
	add	BX,type SPART
	inc	DX
	cmp	DX,PMAX
	jl	short BUFFERCLEARLOOP

	call	_BGM_TIMER_INIT

	;BEEP���[�h�ݒ�
	test	Machine_State,10h	; PC/AT
	jz	short PC98_2
PCAT_2:
	mov	AX,TVALATORG/2
	mov	CX,AX
	mov	AL,0b6h	; CNT#2, L-H WORD, ���`�g, binary
	out	BEEP_MODE_AT,AL		; AT
	mov	AL,CL	; ����
	out	BEEP_CNT_AT,AL		; AT
	mov	AL,CH	; ���, count start
	out	BEEP_CNT_AT,AL		; AT
	jmp	short B_INIT_DONE
	EVEN

PC98_2:
	;bgm_bell_mode(3, (INPB( CLOCK_CHK ) & 0x20) ? 998 : 1229);
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[0501H],80h
	mov	AX,TVAL8ORG/2
	jnz	short CLOCK8MHZ_2
	mov	AX,TVAL10ORG/2
CLOCK8MHZ_2:
	mov	CX,AX
	;OUTB(BEEP_MODE, ((mode & 0x03) << 1) | 0x70);
	mov	AL,3
	shl	AL,1
	or	AL,70h
	mov	DX,BEEP_MODE
	out	DX,AL			; 98
	;OUTB(BEEP_CNT, spval & 0x00ff);
	mov	AL,CL
	mov	DX,BEEP_CNT
	out	DX,AL			; 98
	;OUTB(BEEP_CNT, spval >> 8);
	mov	AL,CH
	out	DX,AL			; 98

B_INIT_DONE:
	mov	glb.init,TRUE
	xor	AX,AX			; BGM_COMPLETE
	MRETURN
endfunc			; }

func BGM_FINISH		; bgm_finish() {
	push	SI
	cmp	glb.init,0
	je	short NOTINITIALIZED2
	call	BGM_STOP_PLAY
	call	BGM_STOP_SOUND
	call	_BGM_TIMER_FINISH
	mov	glb.init,0
NOTINITIALIZED2:

	;�ȃo�b�t�@���
	mov	SI,offset part
FREEMBUF:
	mov	AX,word ptr [SI].mbuf+2
	cmp	AX,0
	je	short NOMBUF
	push	AX
	call	HMEM_FREE
	mov	word ptr [SI].mbuf+2,0
NOMBUF:
	add	SI,type SPART
	cmp	SI,offset part+(type SPART*PMAX)
	jne	short FREEMBUF

	;���ʉ��o�b�t�@���
	mov	SI,offset esound
FREESBUF:
	;if (esound[cnt].sbuf != NULL)
	mov	AX,word ptr [SI].sbuf+2
	cmp	AX,0
	je	short NOSBUF
	push	AX
	call	HMEM_FREE
	mov	word ptr [SI].sbuf+2,0
NOSBUF:
	add	SI,type SESOUND
	cmp	SI,offset esound+(type SESOUND*SMAX)
	jne	short FREESBUF

	pop	SI
	ret
endfunc		; }
END
