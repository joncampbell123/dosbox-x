; master library - VGA - 16color
;
; Description:
;	VGA 16�F, �O���t�B�b�N��ʂ̔C�ӂ̒n�_�̐F�ԍ��𓾂�
;
; Function/Procedures:
;	int vga4_readdot( int x, int y ) ;
;
; Parameters:
;	int	x	�����W(0�`639)
;	int	y	�����W(0�`479)
;
; Returns:
;	int	�F�R�[�h(0�`15)�܂��́A�����W���͈͊O�Ȃ�� -1
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���݂̉�ʃT�C�Y�ŃN���b�s���O���Ă܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/12/ 3 Initial vgcrddot.asm/master.lib 0.22
;	94/ 2/ 7 [M0.22a] bugfix
;	94/ 4/14 [M0.23] vga4_readdot()�ɉ���

	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramLines:WORD
	EXTRN graph_VramWidth:WORD

	.CODE

func VGA4_READDOT	; vga4_readdot() {
	mov	BX,SP
	; ����
	x	= (RETSIZE+1)*2
	y	= (RETSIZE+0)*2

	mov	AX,-1

	mov	CX,SS:[BX+y]
	cmp	CX,graph_VramLines
	jnb	short IGNORE

	mov	DX,graph_VramWidth
	shl	DX,1
	shl	DX,1
	shl	DX,1

	mov	BX,SS:[BX+x]
	cmp	BX,DX
	jnb	short IGNORE

	mov	AX,CX	; CX = seg
	imul	graph_VramWidth
	mov	CX,BX
	and	CX,7
	shr	BX,1
	shr	BX,1
	shr	BX,1
	add	BX,AX
	mov	ES,graph_VramSeg

	mov	DX,VGA_PORT

	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX

	mov	CH,80h		; CH = 0x80 >> (x % 8)
	shr	CH,CL
	mov	CL,0

	mov	AX,VGA_READPLANE_REG or (3 shl 8)
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	mov	AL,CL	; AH=0

IGNORE:
	ret	4
endfunc		; }

END
