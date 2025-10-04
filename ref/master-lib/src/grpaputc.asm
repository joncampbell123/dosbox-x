; superimpose & master library module
;
; Description:
;	
;
; Funcsions/Procedures:
;	void graph_ank_putc( x,y,ank,color )
;
; Parameters:
;	x	�O���t�B�b�N���W x (0�`639)
;	y	�O���t�B�b�N���W y (0�`399)
;	ank	�����R�[�h(0�`255)
;	color	�F(0�`15)
;
; Returns:
;	
;
; Requires:
;	V30 or higher
;	GRCG
;
; Notes:
;	�N���b�s���O���ĂȂ���
;	
;	super.lib 0.22b �� ank_put() �ɑΉ����܂��B
;	�������A������ ank ���|�C���^�ł͂Ȃ��A�������̂��̂�K�v�Ƃ��܂��B
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;	$Id: ankput.asm 0.01 92/06/06 14:29:16 Kazumi Rel $
;	93/ 2/24 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include super.inc
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func GRAPH_ANK_PUTC
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	ank	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	; set color
	mov	AL,0c0h		; RMW MODE
	CLI
	out	7ch,AL
	STI
	mov	DX,7eh
	mov	AH,[BP+color]
	shr	AH,1
	sbb	AL,AL
	out	DX,AL
	shr	AH,1
	sbb	AL,AL
	out	DX,AL
	shr	AH,1
	sbb	AL,AL
	out	DX,AL
	shr	AH,1
	sbb	AL,AL
	out	DX,AL

	mov	ES,graph_VramSeg

	xor	SI,SI
	mov	AX,SI
	mov	AL,[BP+ank]
	add	AX,font_AnkSeg
	mov	DS,AX

	mov	DI,[BP+y]
	mov	BX,DI
	shl	DI,2
	add	DI,BX
	shl	DI,4		; DI *= 80

	mov	CX,[BP+x]
	mov	AX,CX
	shr	AX,3
	add	DI,AX		; DI += x / 8
	and	CX,7
	jz	short NOSHIFT

	mov	DX,16
	EVEN
LINELOOP:
	mov	AH,0
	lodsb
	ror	AX,CL
	stosw
	add	DI,80-2
	dec	DX
	jnz	short LINELOOP
	jmp	short ENDDRAW

NOSHIFT:
	mov	CX,16/2
	mov	AX,80-1
	EVEN
NOSHIFTLOOP:
	movsb
	add	DI,AX
	movsb
	add	DI,AX
	loop	short NOSHIFTLOOP

ENDDRAW:
	mov	AL,0		; GRCG OFF
	out	7ch,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	4*2
endfunc

END
