; master library - VGA
;
; Description:
;	98�G�~�����[�V����: �A�N�Z�X�y�[�W�ݒ�
;	98�G�~�����[�V����: �\���y�[�W�ݒ�
;	98�G�~�����[�V����: GDC�X�N���[��
;
; Functions/Procedures:
;	void at98_accesspage( int page ) ;
;	void at98_showpage( int page ) ;
;	void at98_scroll( unsigned line1, unsigned adr1 ) ;
;
; Parameters:
;	page	0,1	�\���y�[�W�w��
;
;	line1		���̈��line��
;	adr1		���̈�̐擪�A�h���X(16dot���[�h�P��)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT + VGA
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
;	���ˏ��F
;
; Revision History:
;	94/ 6/ 9 Initial: at98scrl.asm/master.lib 0.23
;	94/ 7/ 1 BUGFIX ���������
;	95/ 2/ 1 [M0.23] at98_accesspage()�ǉ�
;	95/ 2/14 [M0.22k] at98_APage�ϐ���ǉ�
;	95/ 4/ 1 [M0.22k] at98_APage, at98_VPage, at98_Offset��at98sc.asm�Ɉړ�

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	at98_VPage:WORD		; at98sc.asm
	EXTRN	at98_APage:WORD		; at98sc.asm
	EXTRN	at98_Offset:WORD	; at98sc.asm

last_lines	dw	0
now_lines	dw	400
nodelay		dw	0

	EXTRN	graph_VramWords:WORD	; grp.asm
	EXTRN	PaletteNote:WORD	; pal.asm
	EXTRN	graph_VramSeg:WORD	; grp.asm
	EXTRN	ClipYT_seg:WORD		; clip.asm

	.CODE
	EXTRN	VGA_DC_MODIFY:CALLMODEL

func AT98_ACCESSPAGE ; at98_accesspage() {
	mov	BX,SP
	;
	newpage = (RETSIZE+0)*2

	mov	AX,SS:[BX+newpage]
	mov	at98_APage,AX

	mov	DX,graph_VramWords
	shr	DX,3
	mul	DX
	add	AX,0a000h
	add	ClipYT_seg,AX
	xchg	graph_VramSeg,AX
	sub	ClipYT_seg,AX
	ret	2
endfunc		; }


func AT98_SHOWPAGE ; at98_showpage() {
	push	BP
	mov	BP,SP
	;
	newpage = (RETSIZE+1)*2
	mov	AX,[BP+newpage]
	mov	at98_VPage,AX
	test	AX,AX
	jz	short	SHOWGO
	mov	AX,graph_VramWords
	shl	AX,1
SHOWGO:
	add	AX,at98_Offset
	mov	BX,AX

DC_PORT	equ	3d4h

	; address
	mov	DX,DC_PORT
	mov	AL,0ch
	mov	AH,BH
	out	DX,AX

	inc	AL
	mov	AH,BL
	out	DX,AX

	; line compare
	mov	BX,now_lines
	cmp	BX,last_lines
	je	short DONE
	mov	last_lines,BX
	dec	BX
	mov	AL,18h
	mov	AH,BL
	out	DX,AX	; low8bit

	push	7
	push	not 10h		; bit8
	shr	BH,1
	sbb	BL,BL
	and	BL,10h
	push	BX
	call	VGA_DC_MODIFY

	push	9
	push	not 40h		; bit9
	shr	BH,1
	sbb	BL,BL
	and	BL,40h
	push	BX
	call	VGA_DC_MODIFY
DONE:

	; VSYNC���ɂȂ�܂ő҂�
	; ����́A98����graph_showpage()�͂ǂ�ȃ^�C�~���O�ł����͂���������
	; VGA�ł̓X�N���[���ɂ��G�~�����[�V�����̊֌W�ŁA����VDISP�ɂȂ���
	; �͂��߂Č��ʂ������̂ŁA�D������ɐ؂�ւ���ƁA�܂��؂�ւ����
	; ���Ȃ��̂ɗ��̂���ŏ����������n�߂Ă��܂��A���ꂪ�������
	; �����тƂȂ��Č���Ă��܂��B�����h�����߂�VSYNC���ł̂ݐ؂�ւ�
	; ��Ƃ��s���悤�ɂ����B94/6/26 ����
	mov	ax,0
	xchg	nodelay,ax
	or	AX,PaletteNote	; �t���Ȃ疳�����ő҂��Ȃ�
	jnz	short delay_skip
	mov	DX,03dah	; VGA���̓��W�X�^1(�J���[)
WAITVSYNC:
	in	AL,DX
	test	AL,8
	jnz	short WAITVSYNC
;	jnz	short delay_skip
;	jnz	short WAITVSYNC_2

WAITVSYNC2:
	in	AL,DX
	test	AL,8
	jz	short WAITVSYNC2

if 0
WAITVSYNC_2:
	in	AL,DX
	test	AL,8
	jnz	short WAITVSYNC_2
endif

delay_skip:
	pop	BP
	ret	2
endfunc		; }

func 	AT98_SCROLL ; at98_scroll() {
	push	BP
	mov	BP,SP
	; 
	line1 = (RETSIZE+2)*2
	adr1 = (RETSIZE+1)*2

	mov	nodelay,1
	mov	AX,[BP+adr1]
	shl	AX,1
	mov	at98_Offset,AX
	mov	AX,[BP+line1]
	mov	now_lines,AX
	push	at98_VPage
	_call	AT98_SHOWPAGE
	pop	BP
	ret	4
endfunc		; }

END
