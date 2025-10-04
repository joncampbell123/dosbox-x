; master library - VGA 16color
;
; Description:
;	�p�^�[���̊g��^�k���\��
;
; Function/Procedures:
;	void vga4_super_zoom_put(int x,int y,int num,
;				unsigned x_rate,int y_rate);
;
; Parameters:
;	x,y	������W
;	num	�p�^�[���ԍ�
;	x_rate	���̔{��(xrate/256�{)
;	y_rate	���̔{��(yrate/256�{) (���Ȃ�Ώc���])
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
;	����
;
; Revision History:
;	93/11/28 Initial: superzpt.asm/master.lib 0.22
;	94/ 6/ 6 Initial: vg4spzpt.asm/master.lib 0.23
;	95/ 1/ 5 [M0.23] y_rate�𕉂ɂ����Ƃ��ɏc�t�]����`
;
	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:WORD,super_patdata:WORD

	.CODE
	extrn	VGC_SETCOLOR:CALLMODEL
	extrn	VGC_BOXFILL:CALLMODEL

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	5*2
	EVEN
	endm

func VGA4_SUPER_ZOOM_PUT	; vga4_super_zoom_put() {
	enter	18,0

	; ����
	org_x	= (RETSIZE+5)*2
	org_y	= (RETSIZE+4)*2
	num	= (RETSIZE+3)*2
	x_rate	= (RETSIZE+2)*2
	y_rate	= (RETSIZE+1)*2

	; ���[�J���ϐ�
	pat_bytes = -2
	y1_pos 	  = -4
	y2_pos 	  = -6
	x_len_256 = -8
	y_len_256 = -10
	b_plane	  = -11
	r_plane	  = -12
	g_plane	  = -13
	i_plane	  = -14
	x_bytes	  = -15
	line_step = -18

	push	SI
	push	DI


; �p�^�[���T�C�Y�A�A�h���X
	mov	BX,[BP+num]
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	[BP+x_bytes],CH	; x�����̃o�C�g��
				; CL �ɂ� ydots
	mov	AL,CH
	mul	CL
	mov	[BP+pat_bytes],AX	; 1�v���[�����̃p�^�[���o�C�g��
	mov	ES,super_patdata[BX]
				; �p�^�[���f�[�^�̃Z�O�����g


	mov	SI,AX		; SI�̓p�^�[�����̃I�t�Z�b�g
				; (�}�X�N�p�^�[�����X�L�b�v)

	mov	AX,0
	test	word ptr [BP+y_rate],8000h
	jns	short NORMAL_V
	; �c�t�]
	neg	word ptr [BP+y_rate]	; y_rate = -y_rate ;
	mov	AL,CH
	add	SI,SI		; SI = SI*2 - x_bytes ;
	sub	SI,AX		; line_step = -x_bytes * 2 ;
	add	AX,AX
NORMAL_V:
	mov	[BP+line_step],AX ; �p�^�[���A�h���X�̎�line�ɐi�ނƂ��̌��Z

	mov	word ptr [BP+y_len_256],128	; �΂�𖳂���
	mov	AX,[BP+org_y]
	xor	CH,CH
	even
for_y:
	mov	BX,[BP+y_len_256]
	add	BX,[BP+y_rate]
	mov	[BP+y_len_256],BL
	test	BH,BH		; ����6�s�ł�y�k���̂Ƃ��ɏ�����
	jnz	short _1	; ���C���̏������X�L�b�v���āA
	mov	BL,[BP+x_bytes]	; ��������}���Ă���B
	add	SI,BX		;
	sub	SI,[BP+line_step]
	loop	short for_y	;
	jmp	return		;
_1:				;
	mov	[BP+y1_pos],AX
	add	AL,BH
	adc	AH,0
	dec	AX
	mov	[BP+y2_pos],AX

	mov	word ptr [BP+x_len_256],128	; �΂�𖳂���
	mov	DI,[BP+org_x]	; DI��x�����̈ʒu
	mov	CH,[BP+x_bytes]
	even
for_x:
	mov	AL,ES:[SI]
	mov	[BP+b_plane],AL
	mov	BX,[BP+pat_bytes]
	mov	AL,ES:[SI + BX]
	mov	[BP+r_plane],AL
	add	BX,BX
	mov	AL,ES:[SI + BX]
	mov	[BP+g_plane],AL
	add	BX,[BP+pat_bytes]
	mov	AL,ES:[SI + BX]
	mov	[BP+i_plane],AL

	push	ES
	push	CX

	mov	CX,8		; 8bit���J�Ԃ�
	even
for_bit:
	push	CX

	; �J���[���v�Z
	shl	byte ptr [BP+i_plane],1
	rcl	AL,1
	shl	byte ptr [BP+g_plane],1
	rcl	AL,1
	shl	byte ptr [BP+r_plane],1
	rcl	AL,1
	shl	byte ptr [BP+b_plane],1
	rcl	AL,1

	mov	BX,[BP+x_len_256]
	add	BX,[BP+x_rate]
	mov	[BP+x_len_256],BL
;	test	BH,BH		; ����2�s��x�k���̂Ƃ��ɏ�����
;	jz	short next_bit	; �h�b�g�̏������Ȃ����߂̂��́c

	mov	DX,DI
	mov	CL,BH
	add	DI,CX

	and	AX,0fh
	jz	short next_bit	; �h�b�g���Ȃ��Ƃ��̓X�L�b�v

	push	DX		; x1
	push	[BP+y1_pos]	; y1
	dec	DI
	push	DI		; x2
	inc	DI
	push	[BP+y2_pos]	; y2

	push	00f0h		; PSET, all plane
	push	AX		; color
	_call	VGC_SETCOLOR
	_call	VGC_BOXFILL

	even
next_bit:
	pop	CX
	loop	short for_bit

next_x:	pop	CX
	pop	ES
	inc	SI
	dec	CH
	jnz	short for_x

	sub	SI,[BP+line_step]
	mov	AX,[BP+y2_pos]
	inc	AX
	even
next_y:	
	;loop	for_y
	dec	CX
	jz	short return
	jmp	for_y

return:
	MRETURN

endfunc		; }

END
