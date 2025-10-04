; master library
;
; Description:
;	�e�L�X�g�Z�~�O���t�B�b�N�̓_����
;
; Function/Procedures:
;	void text_preset( int x, int y ) ;
;
; Parameters:
;	x,y	�_���������W( x=0�`159, y=0�`99 (25�s�̎�) )
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
;	�E�N���b�s���O�͉�����(0�`159)�ƁA��(0�`)�����s���Ă��܂��B
;	�E�����������ꂽ�ꏊ���w�肷��ƁA��������(00h)�ɂ��܂��B
;	�E�_���������Ƃɂ���Ċ��S�ɓ_���Ȃ��Ȃ����̈�́A
;	�@�Z�~�O���t�����𗎂Ƃ��܂��B
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
;	93/ 3/23 bugfix


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PRESET	; text_preset() {
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
	mov	AX,0feh
	rol	AL,CL
	test	byte ptr ES:[BX+2000h],10h
	jz	short NOTCEMI
	and	ES:[BX],AX
	jz	short NOMOREDOT
OUTRANGE:
	pop	BP
	ret	4

	EVEN
NOMOREDOT:	; �h�b�g�������Ȃ����̂Ȃ�Z�~�O���t����������
	and	byte ptr ES:[BX+2000h],not 10h
	pop	BP
	ret	4

	EVEN
NOTCEMI:	; �������\������Ă����̂Ȃ炻�������
	mov	word ptr ES:[BX],0
	pop	BP
	ret	4
endfunc

END
