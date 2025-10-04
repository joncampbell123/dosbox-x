; Hiper library
;
; Description:
;	�N������ PC-98 �}�V���̏��𓾂�B
;
; Procedures/Functions:
;	int get_machine_98( void );
;
; Parameters:
;
; Returns:
;	   bit  3210 : bit 5 �� 1 �̂Ƃ�
;		xxx0 : Note
;		xxx1 : DeskTop
;		xx0x : NEC	#0.07 �ȍ~
;		xx1x : EPSON	#0.07 �ȍ~
;		00xx : Normal mode only    (400 lines)
;		01xx : MATE mode supported (480 lines)
;		1xxx : High-resolustion mode
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	�̂�/V(H.Maeda)
;
; Revision History:
;	93/ 7/19 Initial: Hiper.lib
;	93/07/29 add PC-9821 check
;	93/08/10 add PC-98 color/mono check ?
;		 & _get_machine -> get_machine
;	93/08/25 98 MATE check bugfix
;	93/09/19 ret ������... (^_^;
;	93/10/06 si ���󂵂Ă���(^_^;
;	93/10/22 Mono&Color �`�F�b�N�p�~�ANEC, EPSON �`�F�b�N�ǉ�
;		 �܂��AEPSON note ���`�F�b�N�\
;	93/11/ 3 AX check �ŁA0040:00e0 �`��
;		 16 byte ���� 12 byte check �ɕύX
;	93/12/11 Initial: getmachi.asm/master.lib 0.22 (from hiper.lib)
;	94/01/05 get_machine() ���Aget_machine_at()�Aget_machine_98() �ɕ���
;		 ����ɔ����A���ꂼ��Agetmachi.asm�Aatmachi.asm�A98machi.asm
;		 �Ƀt�@�C���𕪗��B
;	94/02/13 DOS/V Extention/SuperDrivers �� check �ǉ�
;		 GET_MACHINE_AT -> GET_MACHINE_AT2 �ɁB(�b�菈�u)
;	94/03/25 FM-R �� check �ǉ�
;		 GET_MACHINE_98 -> GET_MACHINE_982 �ɁB(�b�菈�u)
;	94/07/11 PC486noteAU �ǉ�
;	94/12/21: [M0.23] getmac92.asm  94-07-12 00:24:40 ���������Ď�荞��
;	95/ 2/28 [M0.22k] DESKTOP�r�b�g�̈Ӗ����t�������̂Ŕ��]


;	PC-9801 �֌W

FMR	EQU	01000000b
PC9801	EQU	00100000b
NOTE98	EQU	00000001b or PC9801	; 0:DeskTop	/ 1:Note
NOTE98_XOR EQU	00000001b		; 1:DESKTOP	/ 0:Note�ɕύX
EPSON	EQU	00000010b or PC9801	; 0:NEC		/ 1:EPSON
LINE480	EQU	00000100b or PC9801	; 0:400line	/ 1:480line(MATE mode)
HIRESO	EQU	00001000b or PC9801	; 0:Normal	/ 1:Hi-resorusion
COLOR	EQU	00010000b or PC9801	; 0:Mono	/ 1:color

	.MODEL SMALL
	.DATA
	EXTRN Machine_State:WORD		; machine.asm

EPSON_NOTES	db	0Dh	; PC286NoteE
		db	16h	; PC286NoteF
		db	20h	; PC386NoteA
		db	22h	; PC386NoteW
		db	27h	; PC386NoteAE
		db	2Ah	; PC386NoteWR
		db	2Eh	; PC386NoteAR
		db	36h	; PC486NoteAS
		db	3Fh	; PC486NoteAU
;		db		; �����ǉ����Ȃ����....
		db	00h	; END of DATA

	include func.inc

	.CODE

	EXTRN	CHECK_MACHINE_FMR:CALLMODEL	; getmacfm.asm
	EXTRN	GET_MACHINE_DOSBOX:CALLMODEL	; getmacdb.asm

func GET_MACHINE_98	; get_machine_98(void)
	_call	CHECK_MACHINE_FMR
	jnz	short NOT_FMR
	mov	AX,FMR
	jmp	short GET_AT_MACHINE_EXIT
NOT_FMR:

;	PC-9801 �֌W

	xor	al,al		; flag clear

	mov	dx,0fff7h
	mov	es,dx
	mov	dx,1827h
	cmp	dx,es:[0000h]
	jne	short EPSON_NORMAL
	or	al,HIRESO or EPSON
	jmp	short PC9821_CHK

EPSON_NORMAL:
	mov	dx,0fd80h
	mov	es,dx
	mov	dx,2a27h	; Normal EPSON check
	cmp	dx,es:[0002h]
	jne	short NEC_MACHINE
	mov	ah,es:[0004h]
	mov	bx,offset EPSON_NOTES
	or	al,EPSON
LOOP_EPSON:
	mov	dl,ds:[bx]
	or	dl,dl
	je	short PC9821_CHK
	inc	bx
	cmp	ah,dl
	jne	LOOP_EPSON
EPSON_NOTE:
	or	al,NOTE98
	jmp	short PC9821_CHK

NEC_MACHINE:
	xor	bx,bx
	mov	es,bx

	or	al,byte ptr es:[501h]
	and	al,08h		; Hireso flag check
	or	al,PC9801	; PC9801 �m�[�}��?

	mov	dh,byte ptr es:[400h]
	and	dh,80h		; Note flag check
	rol	dh,1
	or	al,dh
PC9821_CHK:
	mov	dh,byte ptr es:[045ch]
	and	dh,40h		; PC-9821 flag heck
	ror	dh,1		; dh >> 1
	ror	dh,1		; dh >> 1
	ror	dh,1		; dh >> 1
	ror	dh,1		; dh >> 1
	or	al,dh
	xor	ah,ah
	xor	al,NOTE98_XOR		; note�r�b�g���t�]  ## ���[�� ##
GET_AT_MACHINE_EXIT:
	mov	Machine_State,ax
	jmp	GET_MACHINE_DOSBOX
endfunc

END
