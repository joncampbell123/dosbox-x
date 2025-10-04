; master library
;
; Description:
;	�e�L�X�g�Z�~�O���t�B�b�N�̓_�`��
;
; Function/Procedures:
;	void text_pset( int x, int y ) ;
;
; Parameters:
;	x,y	�_��ł��W( x=0�`159, y=0�`99 (25�s�̎�) )
;
; Returns:
;	none
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
;	�N���b�s���O�͉�����(0�`159)�ƁA��(0�`)�����s���Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 3/20 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PSET	; text_pset() {
	push	BP
	mov	BP,SP
	; ����
	x = (RETSIZE+2)*2
	y = (RETSIZE+1)*2
	mov	AX,[BP+y]
	mov	CX,AX
	and	CX,3
	xor	AX,CX
	js	short OUTRANGE	; y���}�C�i�X�Ȃ�͈͊O
	mov	BX,AX
	shr	AX,1
	shr	AX,1
	add	AX,BX
	shl	AX,1
	add	AX,TextVramSeg
	mov	ES,AX

	mov	BX,[BP+x]
	cmp	BX,160
	jae	short OUTRANGE	; x�� 0�`159�ȊO�Ȃ�͈͊O

	shr	BX,1
	sbb	CH,CH
	and	CH,4
	or	CL,CH
	shl	BX,1
	mov	AX,1
	shl	AX,CL
	test	byte ptr ES:[BX+2000h],10h
	jz	short NOTCEMI
	or	ES:[BX],AX
OUTRANGE:
	pop	BP
	ret	4

	EVEN
NOTCEMI:
	or	byte ptr ES:[BX+2000h],10h
	mov	ES:[BX],AX
	pop	BP
	ret	4
endfunc

END
