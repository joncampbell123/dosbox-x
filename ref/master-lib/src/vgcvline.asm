; master library - VGA - 16color
;
; Description:
;	VGA 16color �������̕`��
;
; Functions/Procedures:
;	void vgc_vline( int x, int y1, int y2 ) ;

; Parameters:
;	int x	x���W
;	int y1	���y���W
;	int y2	����y���W
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;	�Ey1,y2�̏㉺�֌W�͋t�ł����삵�܂��B
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
; �֘A�֐�:
;	grc_setclip(), vgc_setcolor()
;
; Revision History:
;	93/12/ 3 Initial: vgcvline.asm/master.lib 0.22
;	94/ 4/ 8 [M0.23] ��640dot�ȊO�ɂ��Ή�

	.186

	.MODEL SMALL

	.DATA?

	EXTRN	ClipXL:WORD, ClipXR:WORD
	EXTRN	ClipYT:WORD, ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE
	include func.inc
	include vgc.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

retfunc RETURN
	MRETURN
endfunc

func VGC_VLINE	; vgc_vline() {
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	x  = (RETSIZE+3)*2
	y1 = (RETSIZE+2)*2
	y2 = (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	CX,ClipYB

	mov	BX,[BP+y1]
	mov	DX,[BP+y2]
	cmp	BX,DX
	jl	short GO
	xchg	BX,DX		; BX <= DX �ɂ���
GO:

	; y�̃N���b�v
	cmp	DX,CX
	jl	short BOTTOM_OK
	mov	DX,CX
BOTTOM_OK:
	cmp	BX,AX
	jg	short TOP_OK
	mov	BX,AX
TOP_OK:
	sub	DX,BX		; BX = y1, DX = y����
	jl	short RETURN

	mov	AX,[BP+x]	; x�̃N���b�v
	cmp	AX,ClipXL
	jl	short RETURN
	cmp	AX,ClipXR
	jg	short RETURN
	mov	CX,AX
	and	CL,07h
	shr	AX,3
	mov	DI,AX

	mov	AL,80h
	shr	AL,CL

	mov	CX,DX		; CX = y����
	xchg	AX,BX
	mov	BP,graph_VramWidth	; BX������
	imul	BP
	add	DI,AX

	mov	ES,graph_VramSeg ; �Z�O�����g�ݒ�
	inc	CX		; �������o��(abs(y2-y1)+1)

	shr	CX,1
	jnb	short S1
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
S1:
	shr	CX,1
	jnb	short S2
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
S2:	jcxz	short QRETURN
	EVEN
L:	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	test	ES:[DI],BL
	mov	ES:[DI],BL
	add	DI,BP
	loop	short L

QRETURN:
	MRETURN
endfunc
END
