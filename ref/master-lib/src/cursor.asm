; master library - PC98V
;
; Description:
;	�O���t�B�b�N�J�[�\������
;
; Functions/Procedures:
;	void cursor_init(void) ;
;	int cursor_show(void) ;
;	int cursor_hide(void) ;
;	void cursor_moveto( int x, int y ) ;
;	void cursor_pattern( int px, int py, int blc, int whc, void far * pattern ) ;
;
; Parameters:
;	int x,y : �O���t�B�b�N���W�ŐV�����J�[�\���ʒu
;	int px,py : �J�[�\���̃Z���^�[�|�C���g�̈ʒu( 0,0=����, 15,15=�E�� )
;	int blc,whc : �J�[�\���̎n�߂̃p�[�g�̐F�Ǝ��̃p�[�g�̐F
;	void far * pattern : �J�[�\���p�^�[����A���Q�B
;				�ЂƂ�16word�ŁA16x16dot�ˁB
;			�J�[�\���͂��̂Q�����ɏd�˂ĕ`�悷��B
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V Normal
;
; Requiring Resources:
;	CPU: V30�ȏ�
;	GRCG
;
; Notes:
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
;	93/ 2/13 Initial
;	94/ 5/23 VGA�ɂ��Ή�(����)
;	94/ 5/24 bugfix, ���ړ���640dot�ɐ������Ă��̂���ʂɑΉ�������
;	94/ 6/29 bugfix, cursor_moveto() y�̉��[���1dot���Ɉړ��ł��Ă���
;			 cursor_moveto() x,y�����̂Ƃ��ɔ��Β[�ɂ����Ă�
;	94/10/27 bugfix, cursor_show/hide() ���荞�݋֎~�����������̂ŉ���

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramLines:WORD
	EXTRN graph_VramWidth:WORD
	EXTRN Machine_State:WORD

GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_OFF		equ 0
GC_TDW		equ 080h ; ���������ް��͖������āA���ڼ޽��̓��e������
GC_TCR		equ 080h ; ���ڼ޽��Ɠ����F���ޯĂ������ēǂݍ��܂��
GC_RMW		equ 0c0h ; ���������ޯĂ������Ă����ޯĂ����ڼ޽����珑��

mouse_show_count dw -1

	.DATA?

cursor_x	dw	?
cursor_y	dw	?
draw_curx	dw	?
draw_cury	dw	?
draw_adr	dw	?
draw_len	dw	?

center_x	dw	?
center_y	dw	?

white_color	db	?
black_color	db	?

pattern_black	db	2*16 dup (?)
pattern_white	db	2*16 dup (?)

back_pattern	db	3*16*4 dup (?)

black_draw	db	3*16 dup (?)
white_draw	db	3*16 dup (?)

	.CODE
	EXTRN	GET_MACHINE:CALLMODEL

;	void cursor_init(void) ;
func CURSOR_INIT	; {
	_call	GET_MACHINE
	mov	mouse_show_count,-1
	ret
endfunc			; }


; �`��p�p�^�[���̍쐬
; in: DS == DGROUP
; �j�󃌃W�X�^: AX,BX,CX,DX,SI,DI
; 9086 -> �����o�� 9088
; �k���^������ 90812
; 921008
make_draw	proc	; {
	mov	AX,DS
	mov	ES,AX
	mov	SI,offset pattern_black		; white,black����x�ɃV�t�g
	CLD

	mov	AX,cursor_y
	sub	AX,center_y	;	��ɂ͂ݏo�Ă�����
	cwd			;	�J�b�g
	and	DX,AX		; DX=start y(�`0)�}�C�i�X�Ȃ��
	sub	SI,DX		; SI�ɂ��̃}�C�i�X��*2�𑫂�
	sub	SI,DX		;
	sub	AX,DX		; AX�Ƀ}�C�i�X���𑫂�( ���Ȃ�� 0�ɂ��� )
	mov	draw_cury,AX

	mov	BX,DX
	mov	DX,graph_VramWidth
	mul	DX
	mov	draw_adr,AX	; �`��J�n�A�h���Xy��ݒ�

	mov	CX,BX		; �c�̒��������߂�
	add	CX,16		; (DX: ��ɂ݂͂łĂ����畉�� )
	jc	short make_skip1

	mov	CX,graph_VramLines
	sub	CX,16
	sub	CX,draw_cury
	sbb	DX,DX
	and	CX,DX
	add	CX,16
make_skip1:
	mov	draw_len,CX
	mov	CH,CL
	add	CH,16		; �蔲�������A��������x�ɂ��
				; ���ꂾ�ƈ�ڂ�16line����Ă��܂��A
				; �㉺�ɂ͂ݏo�Ă�Ƃ��͖��ʂ����邪�c

	mov	DI,offset black_draw

	mov	AX,cursor_x
	sub	AX,center_x
	mov	draw_curx,AX
	jns	short make_skip2
	neg	AX
	mov	CL,AL
	make_loopl:			; ���[���͂ݏo��ꍇ
		lodsw
		xchg	AH,AL
		shl	AX,CL
		xchg	AH,AL
		stosw
		xor	AL,AL
		stosb
		dec	CH
	jne	make_loopl
	ret
	EVEN

make_skip2:
	mov	CL,AL
	shr	AX,3

	mov	BH,byte ptr graph_VramWidth
	dec	BH
	cmp	AL,BH	;
	sbb	BL,BL	;
	dec	BH
	cmp	AL,BH	;
	sbb	BH,BH	;

	add	draw_adr,AX

	and	CL,7
	je	short make_noshift

	make_loopm:			; x���W��8�̔{���łȂ��Ƃ�
		lodsw
		xchg	AH,AL
		mov	DH,AL
		xor	DL,DL
		shr	DX,CL
		shr	AX,CL
		xchg	AH,AL
		and	AH,BL
		and	DL,BH
		stosw
		mov	AL,DL
		stosb
		dec	CH
	jne	make_loopm
	ret
	EVEN

	make_noshift:		; x���W��8�̔{���̎�
	xchg	CH,CL
	make_loop8:
		lodsw
		and	AH,BL
		stosw
		mov	AL,CH
		stosb
	loop	make_loop8

	ret
	EVEN

make_draw	endp	; }


; GRCG�̃^�C����P�F��ݒ肷��
; IN:
;	AL	�F(0..15)
; �j�󃌃W�X�^:
;	AX,DX
; 921009
grcg_color	proc	; {
	mov	AH,AL
	mov	DX,GC_TILEREG

	shr	AH,1		; B
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; R
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; G
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; I
	sbb	AL,AL
	out	DX,AL

	ret
grcg_color	endp	; }

; (draw_curx,)
; 9086
; �j�󃌃W�X�^:
;	AX,BX,CX,DX,SI,DI,ES
hide	proc	; {
	; ��ʏ�̃}�E�X�J�[�\���ɏd�Ȃ镔����put
	CLD
	mov	BX,draw_adr
	mov	DX,draw_curx
	sar	DX,3
	mov	SI,offset back_pattern
	test	Machine_State,010h	; PC/AT?
	jnz	short hidev
	cmp	DX,78
	mov	DX,graph_VramSeg
	mov	CX,draw_len

	jge	short hide98_next1
	mov	AX,80-3
	hide98_loopseg1:
		mov	ES,DX
		mov	DI,BX
		mov	DX,CX
		hide98_loop1:
			movsw
			movsb
			add	DI,AX
		loop	hide98_loop1
		mov	CX,DX
		mov	DX,ES
		add	DH,8
		or	DH,20h
		cmp	DH,0e8h
	jb	hide98_loopseg1
	jmp	short hide98_next3
	hide98_next1:
	jne	short hide98_next2
	mov	AX,80-2
	hide98_loopseg2:
		mov	ES,DX
		mov	DI,BX
		mov	DX,CX
		hide98_loop2:
			movsw
			inc	SI
			add	DI,AX
		loop	hide98_loop2
		mov	CX,DX
		mov	DX,ES
		add	DH,8
		or	DH,20h
		cmp	DH,0e8h
	jb	hide98_loopseg2
	jmp	short hide98_next3
	hide98_next2:
	mov	AX,80-1
	hide98_loopseg3:
		mov	ES,DX
		mov	DI,BX
		mov	DX,CX
		hide98_loop3:
			movsb
			add	SI,2
			add	DI,AX
		loop	hide98_loop3
		mov	CX,DX
		mov	DX,ES
		add	DH,8
		or	DH,20h
		cmp	DH,0e8h
	jb	hide98_loopseg3

	hide98_next3:
	ret
	EVEN

hidev:
	push	DX
	call	vga_setup
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)	; write directly
	out	DX,AX
	pop	DX

	sub	DX,graph_VramWidth
	neg	DX
	cmp	DX,3
	jle	short hidev1
	mov	DX,3
hidev1:
	mov	ES,graph_VramSeg
	mov	AH,1
hidev_loopseg1:
	push	DX
	mov	DX,SEQ_PORT
	mov	AL,SEQ_MAP_MASK_REG
	out	DX,AX
	pop	DX

	mov	DI,BX
	mov	AL,byte ptr draw_len
hidev_loop1:
	mov	CX,DX
	rep	movsb
	mov	CX,3
	sub	CX,DX
	add	SI,CX
	sub	DI,DX
	add	DI,graph_VramWidth
	dec	AL
	jnz	short hidev_loop1
	shl	AH,1
	cmp	AH,10h
	jb	hidev_loopseg1

	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	mov	DX,SEQ_PORT
	out	DX,AX
	ret
hide	endp	; }

draw	proc	; {
	call	make_draw
	test	Machine_State,010h
	jz	short draw98
	jmp	drawv
draw	endp	; }


; (cursor_x,cursor_y)
; �j�󃌃W�X�^:
;	AX,BX,CX,DX,SI,DI,ES
draw98	proc	; {
	mov	CX,draw_len
	mov	DX,draw_adr

	push	DS

	; �J�[�\���̗���ۑ�
	mov	DI,offset back_pattern
	mov	AX,DS
	mov	ES,AX
	mov	AX,graph_VramSeg
	draw98_loopseg:
		mov	DS,AX
		mov	CH,CL
		mov	SI,DX
		draw98_loop1:
			movsb
			movsw
			add	SI,80-3
			dec	CH
		jne	draw98_loop1
		add	AH,8
		or	AH,20h
		cmp	AH,0e8h
	jb	draw98_loopseg

	pop	DS

	mov	AX,graph_VramSeg
	mov	ES,AX

	; �J�[�\���̍��������̕`��
	mov	AL,0c0h		; RMW mode
	out	GC_MODEREG,AL
	mov	AL,black_color
	mov	DI,DX
	call	grcg_color
	mov	DX,DI
	mov	SI,offset black_draw
	mov	CH,CL
	draw98_loop2:
		movsb
		movsw
		add	DI,80-3
		dec	CH
	jne	draw98_loop2

	; �J�[�\���̔��������̕`��
	mov	AL,white_color
	mov	DI,DX
	call	grcg_color
	mov	SI,offset white_draw
	draw98_loop3:
		movsb
		movsw
		add	DI,80-3
	loop	draw98_loop3

	mov	AL,GC_OFF
	out	GC_MODEREG,AL

draw98_skip:
	ret
draw98	endp	; }


; in: ES:DX = display address
;     SI = pattern address
; break: AX
drawvsub proc ; {
	mov	DI,DX
	mov	DX,VGA_PORT
	mov	AL,VGA_SET_RESET_REG
	out	DX,AX

	mov	DX,DI
	mov	CX,draw_len

	drawv_loop3:
		lodsw
		test	ES:[DI],AL
		stosb
		mov	AL,AH
		test	ES:[DI],AL
		stosb
		lodsb
		test	ES:[DI],AL
		mov	ES:[DI],AL
		sub	DI,2
		add	DI,graph_VramWidth
	loop	drawv_loop3
	ret
drawvsub endp ; }

; break: AX,DX
vga_setup proc ; {
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	mov	DX,SEQ_PORT
	out	DX,AX

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	ret
vga_setup endp ; }

; (cursor_x,cursor_y)
; �j�󃌃W�X�^:
;	AX,BX,CX,DX,SI,DI,ES
drawv	proc	; {
	push	DS

	call	vga_setup

	mov	CX,draw_len
	mov	DX,draw_adr

	; �J�[�\���̗���ۑ�
	mov	DI,offset back_pattern
	mov	AX,DS
	mov	ES,AX
	mov	DS,graph_VramSeg
	mov	AX,VGA_READPLANE_REG or (0 shl 8)
	drawv_loopseg:
		mov	CH,CL
		mov	SI,DX
		mov	DX,VGA_PORT
		out	DX,AX
		mov	DX,SI
		drawv_loop1:
			movsb
			movsw
			sub	SI,3
			add	SI,ES:graph_VramWidth
			dec	CH
		jne	drawv_loop1
		inc	AH
		cmp	AH,4
	jb	drawv_loopseg

	pop	DS

	mov	AX,graph_VramSeg
	mov	ES,AX
	mov	AH,black_color		; �J�[�\���̍��������̕`��
	mov	SI,offset black_draw
	call	drawvsub
	mov	AH,white_color		; �J�[�\���̔��������̕`��
	mov	SI,offset white_draw
	call	drawvsub

drawv_skip:
	ret
drawv	endp	; }


; �J�[�\���̕\��
; 921008
; int cursor_show(void) ;
func CURSOR_SHOW	; {
	mov	AX,mouse_show_count
	inc	AX
	js	short SHOW_SKIP
	jnz	short SHOW_SKIPZ

	push	SI
	push	DI
	call	draw
	pop	DI
	pop	SI
SHOW_SKIPZ:
	xor	AX,AX
SHOW_SKIP:
	mov	mouse_show_count,AX
	ret
endfunc			; }

; �J�[�\���̏���
; int cursor_hide(void) ;
func CURSOR_HIDE	; {
	dec	mouse_show_count
	cmp	mouse_show_count,-1
	jne	short hide_skip

	push	SI
	push	DI
	call	hide
	pop	DI
	pop	SI
hide_skip:
	mov	AX,mouse_show_count
	ret
endfunc			; }


; �J�[�\���̈ʒu�̐ݒ�
;	void cursor_moveto( int x, int y ) ;
func CURSOR_MOVETO	; {
	mov	BX,SP
	; ����
	x	= (RETSIZE+1)*2
	y	= (RETSIZE+0)*2

	pushf
	CLI

	mov	AX,graph_VramWidth
	shl	AX,3
	dec	AX
	mov	CX,SS:[BX+y]
	mov	BX,SS:[BX+x]
	cmp	BH,80h
	sbb	DX,DX
	and	BX,DX
	sub	BX,AX
	sbb	DX,DX
	and	BX,DX
	add	BX,AX
	mov	cursor_x,BX
	mov	AX,graph_VramLines
	dec	AX
	cmp	CH,80h
	sbb	DX,DX
	and	CX,DX
	sub	CX,AX
	sbb	DX,DX
	and	CX,DX
	add	CX,AX
	mov	cursor_y,CX

	cmp	mouse_show_count,0
	jne	short move_skip

	; �V�������W�ƑO�̍��W�������Ȃ�Ή������Ȃ�
	mov	AX,BX
	sub	AX,center_x
	cmp	draw_curx,AX
	jne	short move_hide

	mov	AX,CX
	sub	AX,center_y
	cmp	draw_cury,AX
	je	short move_skip
move_hide:
	push	SI
	push	DI
	call	hide
	call	draw
	pop	DI
	pop	SI
move_skip:
	popf
	ret	4
endfunc			; }

; �O���t�B�b�N�J�[�\���̐ݒ�
;	void cursor_pattern( int px, int py, int blc, int whc, void far * pattern ) ;
; in:
;	pattern: cursor data
;		�J�[�\���f�[�^�́A16word * 2�łȂ�ԁB
;		first 16word: black_mask ( ���̃}�X�N )
;		last 16word: white_mask ( ���̃}�X�N )
;		�����̃}�E�X�h���C�o�͘_�����Z���s��Ȃ��̂�
;		  xor��and�̌��ʂ͂Ȃ��A���ƍ��̈Ӗ������Ȃ��B
func CURSOR_PATTERN	; {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	px	= (RETSIZE+6)*2
	py	= (RETSIZE+5)*2
	blc	= (RETSIZE+4)*2
	whc	= (RETSIZE+3)*2
	pattern	= (RETSIZE+1)*2

	CLD
	mov	AX,[BP+px]
	and	AX,0fh
	mov	center_x,AX
	mov	AX,[BP+py]
	and	AX,0fh
	mov	center_y,AX
	mov	AX,[BP+blc]
	mov	black_color,AL
	mov	AX,[BP+whc]
	mov	white_color,AL

	push	DS
	pop	ES
	lds	SI,[BP+pattern]
	mov	DI,offset pattern_black
	mov	CX,32	; ������x�ɓ]��
pat_loop:
	lodsw
	xchg	AH,AL
	stosw
	loop	short pat_loop
	push	ES
	pop	DS

	cmp	mouse_show_count,0
	jne	short pat_end
	call	hide
	call	draw
pat_end:

	pop	DI
	pop	SI
	pop	BP
	ret	12
endfunc		; }

END
