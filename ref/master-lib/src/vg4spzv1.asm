; master library - VGA 16Color
;
; Description:
;	VGA 16�F, �p�^�[���\�� [�c�g��/�k��][1�v���[��]
;
; Functions/Procedures:
;	void vga4_super_zoom_v_put_1plane(int x,int y,int num,unsigned rate,
;					int pattern_plane,unsigned put_plane);
;
; Parameters:
;	x,y   ���W
;	num   �p�^�[���ԍ�
;	rate  �{��*256�B���Ȃ킿256=���{�B
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
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
;	94/ 7/17 Initial: vg4spzv1.asm/master.lib 0.23
;	95/ 3/28 [M0.22k] MODIFY ��640dot�ȊO�Ή�
;

	.186
	.MODEL SMALL
	include vgc.inc
	include func.inc

	.DATA
	extrn	super_patsize:WORD	; superpa.asm
	extrn	super_patdata:WORD	; superpa.asm
	extrn	graph_VramSeg:WORD	; grp.asm
	extrn	graph_VramWidth:WORD	; grp.asm

	.CODE

func VGA4_SUPER_ZOOM_V_PUT_1PLANE	; vga4_super_zoom_v_put_1plane() {
	enter	4,0
	push	DS
	push	SI
	push	DI

	paramsize = 6*2
	org_x		equ word ptr [BP+(RETSIZE+6)*2]
	org_y		equ word ptr [BP+(RETSIZE+5)*2]
	num		equ word ptr [BP+(RETSIZE+4)*2]
	rate		equ word ptr [BP+(RETSIZE+3)*2]
	pat_plane	equ word ptr [BP+(RETSIZE+2)*2]
	put_plane	equ word ptr [BP+(RETSIZE+1)*2]

	x_bytes		equ word ptr [BP-2]
	x_bytes_l	equ byte ptr [BP-2]
	x_bytes_h	equ byte ptr [BP-1]

	y_dots		equ byte ptr [BP-4]

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX

	mov	BX,put_plane
	mov	AX,BX
	mov	AL,VGA_SET_RESET_REG
	and	AH,0fh
	out	DX,AX
	mov	DX,SEQ_PORT
	mov	AH,BL
	mov	AL,SEQ_MAP_MASK_REG
	not	AH
	and	AH,0fh
	out	DX,AX

; �p�^�[���T�C�Y�A�A�h���X
	mov	BX,num
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	x_bytes_h,0
	mov	x_bytes_l,CH	; x�����̃o�C�g��
	mov	y_dots,CL	; y�����̃h�b�g��

	mov	AL,CH
	mul	CL		; AX = 1�v���[�����̃p�^�[���o�C�g��
	xor	SI,SI		; pattern address offset
	mov	CX,pat_plane
	jcxz	short _4
_3:	add	SI,AX
	loop	short _3
_4:

	mov	AX,graph_VramWidth
	push	DX
	mov	CS:_LINE_ADD,AX
	imul	org_y
	pop	DX
	mov	DI,AX
	mov	AX,org_x
	mov	CX,AX
	shr	AX,3
	add	DI,AX		; GVRAM address offset
	and	CL,7		; shift bit count

; put�J�n
	mov	ES,graph_VramSeg
	mov	DS,super_patdata[BX]
				; pattern address segment
	CLD
	call	disp

return:
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	paramsize
endfunc			; }

disp		proc	near
	push	DI

	mov	BX,128		; y�{���J�E���^
	mov	DH,y_dots
	even
for_y:
	add	BX,rate
	test	BH,BH		; y�k���̂Ƃ��ɏ����郉�C����
	jz	short next_y	; �������X�L�b�v
	even
for_line:
	push	SI
	push	DI
	xor	AX,AX
	mov	CH,x_bytes_l
	even
for_x:
	lodsb
	mov	DL,AL
	shr	AX,CL
	test	ES:[DI],AL
	stosb
	mov	AH,DL
	dec	CH
	jnz	for_x
	xor	AL,AL
	shr	AX,CL
	test	ES:[DI],AL
	stosb
	pop	DI
	pop	SI
	add	DI,1234h
	org $-2
_LINE_ADD dw ?
	dec	BH
	jnz	short for_line
next_y:
	add	SI,x_bytes
	dec	DH
	jnz	short for_y

	pop	DI
	ret
disp		endp

END
