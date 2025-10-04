; superimpose & master library module
;
; Description:
;	�O���t�B�b�N��ʂւ̃p�X�J��������̕`��
;
; Functions/Procedures:
;	procedure graph_font_putp( x,y,cstep:integer; str:string; color:integer );
;
; Parameters:
;	x,y    �ŏ��̕����̍�����W
;	cstep  �S�p�����P�ʂł� 1�����̐i�ݗ�(16�Ō��ԂȂ�)�B
;	       ���p�͔���(�����_�ȉ��؂�̂�)�Ŏg�p�����B
;	str    ������
;	color  �F(0..15)
;
; Returns:
;	none
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: fontputs.asm 0.03 92/06/06 14:30:22 Kazumi Rel $
;
;	93/ 3/20 Initial: grpfputs.asm/master.lib <- super.lib 0.22b
;	94/ 1/ 4 Initial: grpfputp.asm/master.lib 0.22
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

VRAM_WIDTH equ 80
HEIGHT equ 16

	.CODE

retfunc RETURN
	; CG code access
	mov	AL,0ah
	out	68h,AL
	; GRCG off
	xor	AL,AL
	out	7ch,AL		;grcg stop

	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(4+DATASIZE)*2
endfunc

func GRAPH_FONT_PUTP		; graph_font_putp() {
	enter	2,0
	push	SI
	push	DI
	_push	DS

	; ����
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2
	fontseg = -2

	mov	ES,graph_VramSeg

	; GRCG setting
	mov	DX,[BP+color]
	mov	AL,0c0h		;RMW mode
	pushf
	CLI
	out	7ch,AL
	popf
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	AX,font_AnkSeg
	mov	[BP+fontseg],AX
	_lds	SI,[BP+kanji]
	ASSUME DS:NOTHING

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	CLD
	lodsb
	mov	DL,AL		; DL=str length
	mov	DH,0		; DH=���0(�I�[�̑O)
	dec	DX
	js	short RETURN

START:
	mov	BX,CX
	and	CX,7h		;CL=x%8(shift dot counter)  CH=0
	shr	BX,3		;BX=x/8
	add	DI,BX		;GVRAM offset address

	lodsb
	test	AL,0e0h
	jns	short ANKPUT	; 00�`7f = ANK
	jp	short ANKPUT	; 80�`9f, e0�`ff = ����

	test	DX,DX
	jz	short ANKPUT
	dec	DX

	mov	AH,AL

	; CG dot access		;�������ɂ���̂́A�������܂܂Ȃ�������̂Ƃ���
	mov	AL,0bh		; �e�L�X�g��ʂ�����Ȃ��悤�ɂ��邽�߁`
	out	68h,AL

	lodsb
	shl	AH,1
	cmp	AL,9fh
	jnb	short SKIP
	cmp	AL,80h
	adc	AX,0fedfh	; (stc)	; -2,-(20h + 1)
SKIP:	sbb	AL,-2
	and	AX,7f7fh
	out	0a1h,AL
	mov	AL,AH
	out	0a3h,AL
	mov	CH,0
	EVEN

	; CH=line
	; CL=shift
KANJI_LOOP:
	mov	AL,CH
	or	AL,00100000b	;L/R
	out	0a5h,AL
	in	AL,0a9h
	mov	AH,AL

	mov	AL,CH
	out	0a5h,AL
	in	AL,0a9h

	mov	BH,AL
	mov	BL,0
	shr	AX,CL
	shr	BX,CL
	xchg	AH,AL
	mov	ES:[DI],AX
	mov	ES:[DI+2],BL
	add	DI,VRAM_WIDTH	;next line

	inc	CH
	cmp	CH,HEIGHT
	jl	short KANJI_LOOP

	mov	CH,0
	add	CX,[BP+step]
LOOPBACK:
	sub	DI,VRAM_WIDTH*HEIGHT

	dec	DX
	jns	short START
	jmp	RETURN
	EVEN

ANKPUT:
	push	DS
	xor	BX,BX
	mov	AH,BL	; 0
	add	AX,[BP+fontseg]
	mov	DS,AX
	ASSUME DS:NOTHING

ANK_LOOP:
	mov	AL,[BX]
	mov	AH,0
	ror	AX,CL
	mov	ES:[DI],AX
	add	DI,VRAM_WIDTH
	inc	BX
	cmp	BX,HEIGHT
	jl	short ANK_LOOP

	pop	DS
	ASSUME DS:DGROUP

	mov	AX,[BP+step]
	shr	AX,1		;step / 2
	add	CX,AX
	jmp	short LOOPBACK
endfunc		; }

END
