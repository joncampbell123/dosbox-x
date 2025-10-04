; master library - Cx486
;
; Description:
;	Cx486�̃L���b�V�����䃌�W�X�^�̓ǂݏ���, ������
;
; Functions/Procedures:
;	void cx486_read( struct Cx486CacheInfo * rec ) ;
;	void cx486_write( const struct Cx486CacheInfo * rec ) ;
;	void cx486_cacheoff(void) ;
;
; Parameters:
;	none
;
; Returns:
;	rec	�ǂݏ���������
;
;	struct Cx486CacheInfo {
;		int ccr0 ;
;		int ccr1 ;
;		unsigned long ncr[4] ;
;	} ;
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	Cx486�}�V��
;
; Requiring Resources:
;	CPU: Cx486SLC/DLC/DRx2
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
;	���ˏ��F
;
; Revision History:
;	94/ 5/28 Initial: cx486rw.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.CODE

CX486_INDEX	equ 022h
CX486_DATA	equ 023h

CCR0 equ 0c0h
CCR1 equ 0c1h
NCR1 equ 0c4h

CXREAD macro index
	mov	AL,index
	out	CX486_INDEX,AL
	in	AL,CX486_DATA
endm

CXWRITE macro index,data
	mov	AL,index
	out	CX486_INDEX,AL
	mov	AL,data
	out	CX486_DATA,AL
endm

CXREC	struc
_ccr0	dw	?
_ccr1	dw	?
_ncr	dd	4 dup (?)
CXREC	ends

_invd	macro
	db 0fh,08h
	endm

func CX486_READ	; cx486_read() {
	push	BP
	mov	BP,SP
	; ����
	rec = (RETSIZE+1)*2
	_push	DS
	_lds	BX,[BP+rec]

	pushf
	CLI

	mov	AH,0
	CXREAD	CCR0
	mov	[BX]._ccr0,AX

	mov	AH,0
	CXREAD	CCR1
	mov	[BX]._ccr1,AX

	mov	CX,4
	lea	BX,[BX]._ncr

	mov	DL,NCR1
READLOOP:
	CXREAD	DL
	mov	AH,0
	mov	[BX+2],AX
	inc	DL
	CXREAD	DL
	mov	AH,AL
	inc	DL
	CXREAD	DL
	mov	[BX],AX
	add	BX,4
	inc	DL
	loop	short READLOOP

	popf
	_pop	DS
	pop	BP
	ret	(DATASIZE)*2
endfunc		; }

func CX486_WRITE	; cx486_write() {
	push	BP
	mov	BP,SP
	; ����
	rec = (RETSIZE+1)*2
	_push	DS
	_lds	BX,[BP+rec]

	pushf
	CLI

	CXWRITE CCR0,<byte ptr [BX]._ccr0>
	CXWRITE	CCR1,<byte ptr [BX]._ccr1>

	mov	CX,4
	lea	BX,[BX]._ncr

	mov	DL,NCR1
WRITELOOP:
	CXWRITE	DL,[BX+2]
	inc	DL
	CXWRITE	DL,[BX+1]
	inc	DL
	CXWRITE	DL,[BX+0]
	inc	DL
	add	BX,4
	loop	short WRITELOOP

	; flashing
	_invd

	popf
	_pop	DS
	pop	BP
	ret	(DATASIZE)*2
endfunc		; }

func	CX486_CACHEOFF	; cx486_cacheoff() {
	pushf
	CLI
	CXWRITE	CCR0,0
	CXWRITE	CCR1,0

	; cache disble from 00000000h, length 4GB
	CXWRITE	<NCR1+0>,0
	CXWRITE	<NCR1+1>,0
	CXWRITE	<NCR1+2>,0fh

	; �ق�Ƃ͎c���0�Ŗ��߂�񂾂��ǁA�ǂ��ł�������(��������(��))

	; �d�グ
	_invd

	popf
	ret
endfunc			; }

END
