; master library - VGA - 16color
;
; Description:
;	VGA 16color �_�̕`��
;
; Function:
;	void vgc_pset( int x, int y )

; Parameters:
;	int x,y		�_�̍��W
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: V30
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; History:
;	92/6/12	Initial
;	92/7/16 bugfix(^^;
;	94/12/3 Initial: vgcpset.asm/master.lib 0.22
;	94/4/8 [M0.23] ��640dot�ȊO�ɂ��Ή�

	.186
	.MODEL SMALL

	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE
	include func.inc
	include vgc.inc

func VGC_PSET	; vgc_pset() {
	mov	BX,BP	; save BP
	mov	BP,SP

	; parameters
	x = (RETSIZE+1)*2
	y = (RETSIZE+0)*2

	mov	CX,[BP+x]
	mov	AX,[BP+y]

	mov	BP,BX	; restore BP

	cmp	CX,ClipXL
	jl	short RETURN
	cmp	CX,ClipXR
	jg	short RETURN
	cmp	AX,ClipYT
	jl	short RETURN
	cmp	AX,ClipYB
	jg	short RETURN

	imul	graph_VramWidth
	mov	BX,AX			; BX = y vram offset
	mov	AX,CX
	shr	AX,3
	add	BX,AX			; BX = y * graph_VramWidth / 8
	and	CL,7			; CL = x % 8

	mov	ES,graph_VramSeg	; ES = vram segment

	mov	AL,80h
	shr	AL,CL

	test	ES:[BX],AL
	mov	ES:[BX],AL
RETURN:
	ret 4
endfunc		; }
END
