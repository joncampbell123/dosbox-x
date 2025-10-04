; master library - PC98
;
; Description:
;	�O���t�B�b�N��ʂ̔C�ӂ̒n�_�̐F�ԍ��𓾂�
;
; Function/Procedures:
;	int graph_readdot( int x, int y ) ;
;
; Parameters:
;	int	x	�����W(0�`639)
;	int	y	�����W(0�`399)
;
; Returns:
;	int	�F�R�[�h(0�`15)�܂��́A�����W���͈͊O�Ȃ�� -1
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
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
;	92/11/25 Initial
;	93/ 2/ 9 bugfix, clipping
;	93/ 3/ 9 clipping�ł܂��o�O���Ă������[�[�[

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramLines:WORD

	.CODE

func GRAPH_READDOT	; {
	mov	BX,SP
	; ����
	x	= (RETSIZE+1)*2
	y	= (RETSIZE+0)*2

	mov	AX,-1

	mov	DX,SS:[BX+y]
	cmp	DX,graph_VramLines
	jnb	short IGNORE

	mov	BX,SS:[BX+x]
	cmp	BX,640
	jnb	short IGNORE

	mov	AX,DX	; DX = seg
	shl	DX,1
	shl	DX,1
	add	DX,AX
	add	DX,graph_VramSeg
	add	DX,0e000h - 0a800h	; Iplane

	mov	CX,BX
	and	CX,7
	shr	BX,1
	shr	BX,1
	shr	BX,1		; BX = x / 8
	mov	AL,80h		; AL = 0x80 >> (x % 8)
	shr	AL,CL

	mov	ES,DX
	mov	AH,ES:[BX]
	and	AH,AL
	neg	AH
	rcl	CH,1
	sub	DX,0e000h - 0b800h	; I -> G plane

	mov	ES,DX
	mov	AH,ES:[BX]
	and	AH,AL
	neg	AH
	rcl	CH,1
	sub	DX,1000h		; G -> B plane

	mov	ES,DX
	mov	AH,ES:[BX+8000h]	; R plane
	and	AH,AL
	neg	AH
	rcl	CH,1

	and	AL,ES:[BX]		; B plane
	neg	AL
	rcl	CH,1

	mov	AL,CH
	mov	AH,0

IGNORE:
	ret	4
endfunc		; }

END
