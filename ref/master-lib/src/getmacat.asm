; Hiper library
;
; Description:
;	�N������ AT �}�V���̏��𓾂�B
;
; Procedures/Functions:
;	int get_machine_at( void );
;
; Parameters:
;
; Returns:
;	   bit  fedcba9876543210
;		xxxxxxxx0x00xxxx : unknown machine
;		xxxxxxxx0x01xxxx : PC/AT or compatible
;		xxxxxxxx0x10xxxx : PC-9801
;		xxxxxxxx0x11xxxx : unknown machine(���肦�Ȃ�)
;		xxxxxxxx1x00xxxx : FM-R
;		xxxxxxxx1x01xxxx : unknown machine(���肦�Ȃ�)
;		xxxxxxxx1x10xxxx : unknown machine(���肦�Ȃ�)
;		xxxxxxxx1x11xxxx : unknown machine(���肦�Ȃ�)
;
;	   bit  fedcba9876543210 / bit 754 = 001 �̂Ƃ�
;		xxxxxxxxxxxxxxx1 : English mode
;		xxxxxxxxxxxx000x : PS/55
;		xxxxxxxxxxxx001x : DOS/V
;		xxxxxxxxxxxx010x : AX
;		xxxxxxxxxxxx011x : J3100
;		xxxxxxxxxxxx100x : DR-DOS
;		xxxxxxxxxxxx101x : MS-DOS/V
;		xxxxxxxxxxxx110x : (reserved)
;		xxxxxxxxxxxx111x : (reserved)
;		xxxxxxx0xxxxxxxx : ansi.sys �Ȃ�
;		xxxxxxx1xxxxxxxx : ansi.sys ����
;		xxxxxx0xx0xxxxx0 : Japanese mode(no DOS/V EXTENSION /
;						 no SUPERDRIVERS(32) )
;		xxxxxx0xx1xxxxx0 : Japanese mode(DOS/V EXTENSION)
;		xxxxxx1xx0xxxxx0 : Japanese mode(SUPERDRIVERS(32))
;		xxxxxx1xx1xxxxx0 : (reserved)(���肦�Ȃ�)
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	PC/AT
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
;	�̂�(H.Maeda)
;
; Revision History:
;	93/12/23 Initial: Hiper.lib(getmachi.asm �̃\�[�X�͖Y��悤)
;	93/12/30 AX�AJ3100 �`�F�b�N�������B
;	94/02/13 DOS/V Extention/SuperDrivers �� check �ǉ�
;		 GET_MACHINE_AT -> GET_MACHINE_AT2 �ɁB(�b�菈�u)
;	94/03/25 FM-R �� check �ǉ�
;		 GET_MACHINE_98 -> GET_MACHINE_982 �ɁB(�b�菈�u)
;	94/07/10 ansi.sys install check �ǉ�
;	94/12/21: [M0.23] getmaca2.asm  94-07-10 23:59:20 ���������Ď�荞��
;	95/ 2/24 [M0.22k] SuperDrivers��int 15h�Ŋ��荞�݋֎~���Ă�̂ɑΏ�
;	95/ 3/17 [M0.22k] NT��DR-DOS�ƔF�����Ă��܂��Ă����̂Ŕ����ύX�B
;			  ������Novell DOS��DR-DOS�����B
;	95/ 5/30 PC-DOS/V 6.30 �̓o��ɂ�� V-TEXT CHECK �������Ȃ��Ȃ���
;		 �����̂ŉ��C�B
;		 �� ���܂ł� DOSVEXTENSION �ŁADOS/V EXTENSION ��
;		 SuperDrivers(32) ��F�����Ă������A����̉��C�ŁA
;		 �]���� DOSVEXTENTION �� VTEXT �Ƃ��A
;		 �� V-TEXT driver �����ꂼ�� DOSVEXTENSION �� SUPERDRIVES ��
;		 ���ĔF������悤�ɕύX�����B

;	PC/AT �֌W

;		fedcba9876543210
FMR	EQU	0000000010000000b
PCAT	EQU	0000000000010000b
PS55JP	EQU	0000000000000000b or PCAT	; PS/55 ���{��DOS
PS55US	EQU	0000000000000001b or PCAT	; PS/55 �p��DOS
DOSVJP	EQU	0000000000000010b or PCAT	; DOS/V ���{��Ӱ��
DOSVUS	EQU	0000000000000011b or PCAT	; DOS/V �p��Ӱ��
AXJP	EQU	0000000000000100b or PCAT	; AX ���{��Ӱ��
AXUS	EQU	0000000000000101b or PCAT	; AX �p��Ӱ��
J3100JP	EQU	0000000000000110b or PCAT	; J3100 ���{��Ӱ��
J3100US	EQU	0000000000000111b or PCAT	; J3100 �p��Ӱ��
DRDOSJP	EQU	0000000000001000b or PCAT	; DR-DOS ���{��Ӱ��
DRDOSUS	EQU	0000000000001001b or PCAT	; DR-DOS �p��Ӱ��
MSDOSVJ	EQU	0000000000001010b or PCAT	; MS-DOS/V ���{��Ӱ��
MSDOSVU	EQU	0000000000001011b or PCAT	; MS-DOS/V �p��Ӱ��

;		fedcba9876543210
JAPAN	EQU	0000000000000000b or PCAT	; ���{��
ENGLISH	EQU	0000000000000001b or PCAT	; �p��
DOSVEX	EQU	0000000001000000b		; DOS/V Extention
SUPERD	EQU	0000001000000000b		; SUPERDRIVERS(32)

ANSISYS	EQU	0000000100000000b or PCAT	; ANSI.SYS

	.MODEL SMALL
	.DATA

	EXTRN Machine_State:WORD		; machine.asm

IBMADSP	db	'$IBMADSP',0		;$DISP�n�̃f�o�C�X��
IBMAFNT	db	'$IBMAFNT',0		;$DISP�n�̃f�o�C�X��
EXENTRY	dd	0			; Extention Entry Address

	include func.inc

	.CODE

	EXTRN	CHECK_MACHINE_FMR:CALLMODEL	; getmacfm.asm
	EXTRN	GET_MACHINE_DOSBOX:CALLMODEL	; getmacdb.asm

func GET_MACHINE_AT	; get_machine_at() {
	_call	CHECK_MACHINE_FMR
	jnz	short NOT_FMR
	mov	AX,FMR
	jmp	GET_AT_MACHINE_EXIT
NOT_FMR:
	xor	BX,BX
	mov	AX,4F01h
	int	2Fh

	or	BX,BX		; BX �� 0 �Ȃ�
	je	short DOSV_OR_DRDOS_OR_AX	; DOS/V �܂��� DR-DOS �܂��� AX

				; ����ȊO�Ȃ� MS-DOS/V �� J3100 ����
;	J-3100 �� check

	push	ds
	mov	ax,0f000h
	mov	ds,ax
	mov	al,byte ptr ds:[0e010h]	; F000:E010�ɁA'T' �����݂��邩?
	cmp	al,'T'			; F000:E010 = 'TOSHIBA'
	jne	short MSDOSV
	xor	ax,ax
	mov	ds,ax
	mov	al,byte ptr ds:[04D0h]
	not	al
	and	al,00000001b	; english flag check
	or	al,J3100JP
	xor	ah,ah
	pop	ds
	jmp	GET_AT_MACHINE_EXIT

MSDOSV:
	pop	ds
MSDOS2:
	mov	bx,MSDOSVJ	; MS-DOS/V �̓��{��/�p�� check �͕������
	jmp	short CHK_DBCS	; �����ƁA DOS/V �Ɠ�������낤(^_^;

DOSV_OR_DRDOS_OR_AX:
;	AX mode �� check
	xor	BX,BX
	mov	AX,5001h
	int	10h
	or	BL,BL		; BL �� 0 �Ȃ� AX ����Ȃ�(DOS/V �� DR-DOS)
	je	short DOSV_OR_DRDOS	; AX �Ȃ� country code �������Ă���B

;	AX �̏ꍇ

	mov	AX,AXJP		; AX ���{�ꃂ�[�h
	cmp	bx,0051h	; country code 081 �Ȃ� skip
	je	GET_AT_MACHINE_EXIT
	or	ax,bx		; country code 01 �Ȃ� or ���� English ���[�h
	jmp     GET_AT_MACHINE_EXIT

DOSV_OR_DRDOS:
;	DR-DOS �� check
	mov	ax,3000h
	int	21h
	cmp	bh,0ffh
	je	MSDOS2		; 0ffh=Microsoft
	and	bh,not 1	; 0eeh=DR-DOS, 0efh=Novell DOS
	cmp	bh,0eeh
	jne	short DOSV
	mov	dl,DRDOSJP	; DR DOS �p��
	jmp	short CHK_DBCS

DOSV:				; DOS/V or PS/55
	mov     ax,4900h
	pushf
	int	15h		; BIOS �^�C�v�̎擾
	sbb	AX,AX
	popf
	test	AX,AX
	jnz	NOT_DOSV
	or	bl,bl
	jnz	NOT_DOSV
	mov	bx,DOSVJP	; set DOS/V

;	DOS/V �� check
CHK_DBCS:
	push	ds
	push	si
	xor	ax,ax
	mov	ds,ax
	mov	ax,6300h	; get DBCS vector
	int	21h
	mov	ax,ds:[si]	; ���{��Ȃ� not 0
	not	ax
	and	ax,01h
	or	bx,ax			; bx = get_video_mode
	pop	si
	pop	ds


	; DOS/V EXTENTION CHECK	by �̂�		@95.05.28 ��������

	pushf
	push	BX
	mov	AX,5010h		; DOS/V EXTENTION �̑���check
	int	15H
	cmp	AH,86h
	jnz	SUPPORTED
	xor	AX,AX			; AX=0000(not support)
	jmp	short NOT_SUPPORT
SUPPORTED:
	mov	AX,ES:[BX+02h]		; AX=0000(no Ext.) =0001(Ext.)
	and	AX,01
NOT_SUPPORT:
	pop	BX
	popf				; DOSVEX=AX=0000:0000:0100:0000
	jc	short EXIST_CHK_SD	;	       C    8    4    0
					;	 AX=0000:0000:0000:0001
	ror	AL,1			;	 AX=0000:0000:1000:0000
	ror	AL,1			;	 AX=0000:0000:0100:0000
	or	AX,BX			; AX = get_viodeo_mode
	test	AX,DOSVEX		; DOS/V Extention?
	je	short EXIST_CHK_SD	; go NO DOS/V EXTENSION

	; DOS BOX CHECK by �̂�
				; DOS BOX �ł́AEXTENSION ������悤��
				; ������݂����Ȃ̂ŁA���̑Ώ��B
	mov	BX,AX
	mov	DX,offset IBMADSP	; V-Text device name
	mov	AX,3D00h		; '$IBMADSP'
	int	21h			; open device
	jnc	short EXIST_IBMADSP
	xor	BX,DOSVEX		; reset DOX/V EXTENSION
	mov	AX,BX
	jmp	short GET_AT_MACHINE_EXIT
EXIST_IBMADSP:
	push	BX
	mov	BX,AX
	mov	AH,3Eh
	int	21h			;close device
	pop	BX
	mov	AX,BX
	jmp	short GET_AT_MACHINE_EXIT

	; SUPERDRIVERS(32) CHECK
EXIST_CHK_SD:

	push	BX
	mov	DX,offset IBMAFNT	; V-Text device name
	mov	AX,3D00h		; '$IBMAFNT'
	int	21h			; open device
	jc	ERR_EXIT_SD		; Error(=SuperDrivers)

	push	AX
	mov	DX,offset EXENTRY	; API Entry address
	mov	CX,04
	mov	BX,AX
	mov	AX,4402h
	int	21h			; IOCTL read
	pop	AX

	pushf
	mov	BX,AX
	mov	AH,3Eh
	int	21h			;close device
	popf

	jc	ERR_EXIT_SD

	mov	AX,50f1h
	call	EXENTRY
	or	AH,AH
	jnz	short ERR_EXIT_SD
	pop	BX
	or	BX,SUPERD		; or SUPERDRIVERS(32)
	mov	AX,BX
	jmp	short GET_AT_MACHINE_EXIT

ERR_EXIT_SD:
	pop	BX
	mov	AX,BX
	jmp	short GET_AT_MACHINE_EXIT

;						@95.05.28 �����܂�

;	PS/55 �� check
NOT_DOSV:
	push	ds
	xor     ax,ax
	mov     ds,ax
	mov	ax,ds:[7Dh*4]	; INT 7Dh �̎g�p�̃`�F�b�N
	or	ax,ds:[7Dh*4+2]
	pop	ds

	mov	ax,PS55JP	; JDOS ���{��Ӱ��
	jnz	short GET_AT_MACHINE_EXIT
	mov	ax,PS55US	; PC-DOS

GET_AT_MACHINE_EXIT:
	mov	DX,AX		; dx �ɑҔ�

	; ansi.sys �� check

	mov	AX,01a00h	; ansi.sys �� ID
	xor	BX,BX
	xor	CX,CX
	int	2Fh		; AL = 0ffh �Ȃ� ansi.sys ����
	mov	AH,AL		; AX = 0ffffh �Ȃ� ansi.sys ����
	cmp	AX,0ffffh
	jne	short NOT_ANSI_SYS
	and	AX,ANSISYS
	or	DX,AX
NOT_ANSI_SYS:
	mov	Machine_State,DX
	mov	AX,DX
	jmp	GET_MACHINE_DOSBOX
endfunc			; }

END
