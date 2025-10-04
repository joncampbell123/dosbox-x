; master library - PC98
;
; Description:
;	RS-232C���C�u�����̏������^�I���A���荞�ݏ���
;
; Function/Procedures:
;	void sio_start( int port, int speed, int param, int flow ) ;
;	void sio_end(int port) ;
;	void sio_leave(int port) ;
;	void sio_setspeed( int port, int speed ) ;
;	void sio_enable( int port, ) ;
;	void sio_disable( int port ) ;
;
; Parameters:
;	speed	�{�[���[�g�B0=��ݒ�, SIO_150�`SIO_38400=�ݒ�
;	param	�ʐM�p�����[�^�BSIO_N81�Ȃ�
;	flow	�t���[����̕��@�B0 = none, 1 = RS/CS, 2 = XON/XOFF
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	sio_start�����s������A�A�v���P�[�V�����I���O��sio_end��K���ĂԂ��ƁB
;	sio_start���Asio_end�����s����O�ɍĎ��s����ƁA�p�����[�^�ނ̕ύX��
;	�s���݂̂ƂȂ�B
;	sio_end�̑����sio_leave���g���Ɗe�M����OFF���s��Ȃ��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/07 Initial
;	93/ 1/31 sio_leave�ǉ��B75bps�p�~�B
;	93/ 2/12 sio_check_cs�p�~(���Ă�������Ȃ�����,���荞�݂��ς��������)
;	93/ 2/17 ���荞�݃}�X�N�̑ޔ�/����
;	93/ 2/19 sio_start, mode��������Ȃ����[�h����
;	93/ 7/19 ���荞�݋���(STI)��r������pushf->cli->popf�ɂ����B
;	93/ 7/23 [M0.20] ����port�ǉ��B���g�͂܂��Ȃ��B
;	93/ 9/11 [M0.21] ���x�ɂ� SIO_MIDI (SIO_MIDISPEED, 31250bps)�ǉ��B
;	93/ 9/11 [M0.21] �n�C���]�ɑΉ���������B���m�F����
;			���_���B����ǂ͂ǂ���?
;	93/10/28 [M0.21] sio_bit_off��bugfix������A�����������܂��܂�������
;			���̂������Ȃ��Ȃ��Ă�(^^; �̂ŏC��
;	95/5/29  [M0.23] 8MHz�n�ł͂�����������20800bps�ɂ���
;			(SIO_19200���w�肷���20800bps�ɂȂ�)
;
	.MODEL SMALL
	include func.inc
	include sio.inc

	.DATA?
old_intvect	dd	?
old_intmask	db	?
old_intmask_pic	db	?

	.CODE
	EXTRN DOS_SETVECT:CALLMODEL
	EXTRN SIO_BIT_OFF:CALLMODEL


SIO_INT proc far	; {
	push	AX
	in	AL,SYSTEM_PORTC
	push	BX
	push	DS

	push	AX
	and	AL,not SIO_INTMASKS
	out	SYSTEM_PORTC,AL		; �ʐM���荞�݋֎~

	mov	AX,seg DGROUP
	mov	DS,AX

	in	AL,SIO_STAT
	test	AL,SIO_STAT_RXRDY
	jz	short SI_SENDF

	; ��M���荞�� -----------------------------

	in	AL,SIO_DATA		; AL = read data

	cmp	sio_flow_type,SIO_FLOW_SOFT
	jne	short SIREC
	; �\�t�g�E�F�A�t���[����̂Ƃ��́AXOFF/OFF�̎�M�ɂ���āc
	cmp	AL,XOFF
	je	short REC_STOP

	cmp	sio_send_stop,1		; �~�߂��Ă��Ȃ��Ȃ�A���ʂ̎�M��
	jne	short SIREC
		; ������
	cmp	AL,XON			; XOFF���󂯂Ď~�܂��Ă�Ƃ���
	jne	short SI_SEND		; XON�ȊO�̎�M�����͖�������

	pop	AX
	or	AL,SIO_TXRE_BIT		; ���M���荞�݋���
	push	AX
	dec	sio_send_stop		; ���M��
	jmp	short SI_SEND
	EVEN

REC_STOP:	; ��~�܂�
	pop	AX
	and	AL,not SIO_TXRE_BIT	; ���M���荞�݋֎~
	push	AX
	mov	sio_send_stop,1		; ����Ɏ~�߂�ꂽ
	jmp	short SI_SEND
	EVEN

SIREC:	; ��M�o�b�t�@�i�[�O�̌���
	mov	BX,sio_ReceiveLen
	cmp	BX,FLOW_STOP_SIZE	; �t���[�J�n?
	jb	short REC_STORE		; ���Ȃ��Ȃ疳�����Ɋi�[����

	; ��~�v�����o
	cmp	sio_flow_type,1		; SIO_FLOW_SOFT:2
	je	short REC_HARD		; SIO_FLOW_HARD:1
	jb	short REC_CHECKFULL	; SIO_FLOW_NO:0
		; �\�t�g�t���[
		mov	sio_rec_stop,XOFF		; ��~�v��
		jmp	short REC_CHECKFULL
		EVEN
REC_HARD:
		; �n�[�h�t���[
		push	AX
		xor	AX,AX
		push	AX
		mov	AL,SIO_CMD_RS
		push	AX
		call	SIO_BIT_OFF
		pop	AX
		mov	sio_rec_stop,1 ; �������ɑ��M��~��v��������flag

REC_CHECKFULL:
	cmp	BX,RECEIVEBUF_SIZE
	je	short SI_SEND		; ���t�Ȃ疳�������

REC_STORE:
	mov	BX,sio_receive_wp
	mov	[BX+sio_receive_buf],AL	; �i�[
	RING_INC BX,RECEIVEBUF_SIZE,AX	; (��M)�������݃|�C���^�X�V
	mov	sio_receive_wp,BX
	inc	sio_ReceiveLen

		; �����ł܂��X�e�[�^�X�ǂ�ł�̂́A�����܂ł̊Ԃ�
		; ���M�\�ɕς�����Ƃ��ɂ���悭�΁c�Ƃ����킯
SI_SEND:
	in	AL,SIO_STAT
SI_SENDF:
	test	AL,SIO_STAT_TXRDY	; ���M?
	jz	short SI_EXIT

	; ���M���荞�� ----------------------
	cmp	sio_flow_type,SIO_FLOW_SOFT
	jne	short SI_SEND_1

	; �\�t�g�t���[����
SI_SEND_SOFT:
	mov	AX,sio_rec_stop
	cmp	AX,1
	jle	short SI_SEND_1
	; ���荞�݂�������������Ȃ狭�����M
SI_XSEND:
	out	SIO_DATA,AL		; XON/XOFF���M�����
	mov	sio_rec_stop,1
	cmp	AL,XOFF			; XOFF�Ȃ��M��~�t���O�𗧂ĂāA
	je	short SI_EXIT
	mov	sio_rec_stop,0		; XON�Ȃ�Ύ�M���Ȃ̂��
	jmp	short SI_EXIT
	EVEN

	; ���M�o�b�t�@����ɂȂ����Ƃ�
SI_EMPTY:
	mov	sio_send_stop,2		; �o�b�t�@��
SI_STOP:
	mov	AL,20h			; EOI
	out	0,AL
	pop	AX
	and	AL,not SIO_TXRE_BIT	; ���M���荞�݋֎~
	out	SYSTEM_PORTC,AL		; ���M���荞�݃t���O�ݒ�

	pop	DS
	pop	BX
	pop	AX
	iret

	EVEN
SI_SEND_1:
	xor	AX,AX
	cmp	sio_send_stop,AX
	jnz	short SI_STOP		; ��~���Ȃ瑗��Ȃ�
	cmp	sio_SendLen,AX
	je	short SI_EMPTY		; �o�b�t�@����Ȃ炨��肾��
	mov	BX,sio_send_rp		; ���M�o�b�t�@�������Ă���
	mov	AL,[BX+sio_send_buf]	; �o�b�t�@����AL�Ɏ��
	out	SIO_DATA,AL		; ���M�����Ⴄ
	RING_INC BX,SENDBUF_SIZE,AX	; �|�C���^�X�V
	mov	sio_send_rp,BX
	dec	sio_SendLen
	je	short SI_EMPTY
	pop	AX
	or	AX,SIO_TXRE_BIT		; ���M����
	push	AX
SI_EXIT:
	mov	AL,20h			; EOI
	out	0,AL
	pop	AX
	out	SYSTEM_PORTC,AL		; ���M���荞�݃t���O�ݒ�

	pop	DS
	pop	BX
	pop	AX
	iret
SIO_INT endp		; }

func SIO_START		; sio_start() {
	push	BP
	mov	BP,SP
	pushf

	; ����
	port	= (RETSIZE+4)*2
	speed	= (RETSIZE+3)*2
	param	= (RETSIZE+2)*2
	flow	= (RETSIZE+1)*2

	in	AL,SYSTEM_PORTC		; �V�X�e���|�[�g��ǂ�
	and	AL,SIO_INTMASKS
	mov	CX,AX			; CL = ���̊��荞�݃}�X�N

	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE

	; ���[�N�G���A���N���A
	xor	AX,AX
	mov	sio_send_rp,AX
	mov	sio_send_wp,AX
	mov	sio_SendLen,AX
	mov	sio_receive_rp,AX
	mov	sio_receive_wp,AX
	mov	sio_ReceiveLen,AX
	mov	sio_send_stop,2
	mov	sio_rec_stop,AX

	cmp	byte ptr sio_cmdbackup,AL
	je	short START_FIRST
	; �Q�x�ڈȍ~

	; ���x�̐ݒ�
	push	[BP+port]
	push	[BP+speed]
	call	CALLMODEL PTR SIO_SETSPEED
	jmp	short START_RESET
	EVEN

START_FIRST:
	; ���荞�݃x�N�^�̓o�^
	mov	old_intmask,CL		; ���荞�݃}�X�N�̑ޔ�
	in	AL,02h
	or	AL,not 00010000b	; 8251A�̊��荞�݃}�X�N��ۑ�
	mov	old_intmask_pic,AL

	mov	AX,SIO_INTVECT
	push	AX
	push	CS
	mov	AX,offset SIO_INT
	push	AX
	call	DOS_SETVECT
	mov	word ptr old_intvect,AX
	mov	word ptr old_intvect+2,DX

	; ���x�̐ݒ�
	push	[BP+port]
	push	[BP+speed]
	call	CALLMODEL PTR SIO_SETSPEED

	; �t���[������@�̐ݒ�
	mov	AX,[BP+flow]
	mov	sio_flow_type,AX

	mov	AL,[BP+param]
	and	AL,11111100b		; ���[�h���[�h
	jz	short SKIP_MODE
	mov	BL,AL

	; 8251�����Z�b�g����
	; �菇
	CLI
	xor	AX,AX
	out	SIO_CMD,AL
	jmp	$+2
	out	SIO_CMD,AL
	jmp	$+2
	out	SIO_CMD,AL
	jmp	$+2
START_RESET:
	CLI
	mov	AL,40h			; RESET
	out	SIO_CMD,AL
	jmp	$+2

	; ���[�h
	mov	AL,BL
	or	AL,2			; x16
	out	SIO_MODE,AL
	jmp	$+2

SKIP_MODE:
	; �R�}���h
	mov	AL,00110111b		; ? RESET RS ERRCLR  SBRK RXEN ER TXEN
	out	SIO_CMD,AL
	mov	byte ptr sio_cmdbackup,AL

	in	AL,02h
	and	AL,not 00010000b	; 8251A�̊��荞�݂�����
	out	02h,AL

	popf
	push	[BP+port]
	call	CALLMODEL PTR SIO_ENABLE
	pop	BP
	ret	8
endfunc			; }

func SIO_END		; sio_end() {
	push	BP
	mov	BP,SP
	port = (RETSIZE+1)*2

	cmp	byte ptr sio_cmdbackup,0
	jne	short END_GO
	pop	BP
	ret	2
END_GO:
	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE
	; �e������� OFF
	xor	AX,AX
	push	AX
	mov	AX,SIO_CMD_ER or SIO_CMD_RS or SIO_CMD_BREAK
	push	AX
	call	SIO_BIT_OFF
	jmp	short LEAVE_GO
endfunc			; }

func SIO_LEAVE		; sio_leave() {
	push	BP
	mov	BP,SP
	port = (RETSIZE+1)*2

	cmp	byte ptr sio_cmdbackup,0
	je	short LEAVE_IGNORE
	push	[BP+port]
	call	CALLMODEL PTR SIO_DISABLE
LEAVE_GO:
	pushf
	CLI
	in	AL,02h
	or	AL,00010000b		; 8251A�̊��荞�݂��}�X�N
	out	02h,AL
	popf

	; ���荞�݃x�N�^�̕���
	mov	AX,SIO_INTVECT
	push	AX
	push	word ptr old_intvect+2
	push	word ptr old_intvect
	call	DOS_SETVECT
	mov	sio_cmdbackup,0

	; ���荞�݃}�X�N�̕���
	pushf
	CLI

	in	AL,SYSTEM_PORTC
	jmp	short $+2
	or	AL,old_intmask
	out	SYSTEM_PORTC,AL

	in	AL,02h
	and	AL,old_intmask		; 8251A�̊��荞�݃}�X�N�𕜌�
	out	02h,AL

	popf

LEAVE_IGNORE:
	pop	BP
	ret	2
endfunc			; }

func SIO_SETSPEED	; sio_setspeed() {
	mov	BX,SP
	; ����
	port	= (RETSIZE+1)*2
	speed	= RETSIZE*2
	mov	AX,SS:[BX+speed]
	test	AX,AX
	jz	short SPEED_IGNORE

	mov	CX,AX

	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h
	jz	short SPEED_5MHz	; 5MHz�n? 8MHz�n?
	; 8MHz�n(�^�C�}��2MHz)
	mov	AX,8		; 8(20800bps)������ɂ���
	mov	BX,1664
	mov	DX,4		; RS-MIDI 31250bps�ߎ��l�p
	jmp	short SPEED_1
SPEED_5MHz:
	; 5MHz�n(�^�C�}��2.5MHz)
	mov	AX,9		; 9(38400bps)������ɂ���
	mov	BX,2048
	mov	DX,5		; RS-MIDI 31250bps�ߎ��l�p
SPEED_1:
	cmp	CX,SIO_MIDISPEED
	je	short MIDI
	sub	CX,AX		; CX = min(CX,AX)
	sbb	DX,DX
	and	CX,DX
	add	CX,AX
	shr	BX,CL		; BX = �^�C�}#2�̕����l
	mov	DX,BX
MIDI:

	pushf
	CLI
	mov	AL,0b6h		; �J�E���^#2�����[�h�R��
	out	77h,AL
	jmp	$+2
	jmp	$+2
	mov	AL,DL
	out	75h,AL		; ����
	jmp	$+2
	jmp	$+2
	mov	AL,DH
	out	75h,AL		; ���
	popf
SPEED_IGNORE:
	ret 4
endfunc			; }

func SIO_ENABLE		; sio_enable() {
	pushf
	CLI
	in	AL,SYSTEM_PORTC
	jmp	$+2
	and	AL,not SIO_INTMASKS
	or	AL,SIO_TXRE_BIT or SIO_RXRE_BIT
	out	SYSTEM_PORTC,AL
	popf
	ret	2
endfunc			; }

func SIO_DISABLE	; sio_disable() {
	pushf
	CLI
	in	al,SYSTEM_PORTC		; �V�X�e���|�[�g��ǂ�
	jmp	$+2
	and	al,not SIO_INTMASKS
	out	SYSTEM_PORTC,al		; RS-232C�̑S���荞�݂��֎~
	popf
	ret	2
endfunc			; }

END
