; master library
;
; Description:
;	�e�L�X�g�Z�~�O���t�B�b�N�̐���16�h�b�g�C���[�W�`��(������)
;
; Function/Procedures:
;	void text_worddota( int x, int y, unsigned image, unsigned dotlen, unsigned atr ) ;
;
; Parameters:
;	x,y	�J�n���W( x= -15�`159, y=0�`99 (25�s�̎�) )
;	image	�`�悷��16bit�C���[�W(�ŏ�ʃr�b�g�����[)
;		�����Ă���r�b�g�̏��̓Z�b�g���A
;		�����Ă���r�b�g�̏��̓��Z�b�g���܂��B
;	dotlen	�`�悷��h�b�g�� (16�𒴂���ƁAimage���J��Ԃ��܂�)
;	atr	�Z�b�g���镶������
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
;	�N���b�s���O�͉�����(-15�`159)�ƁA��(0�`)�����s���Ă��܂��B
;	���̊֐��ł́A�S�Ẵr�b�g�����������[�h�́A�Z�~�O���t������
;	���Ƃ��Ă��܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 3/25 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	10
	EVEN
	endm

retfunc OUTRANGE
	MRETURN
endfunc

func TEXT_WORDDOTA	; text_worddota() {
	push	BP
	mov	BP,SP
	; ����
	x     = (RETSIZE+5)*2
	y     = (RETSIZE+4)*2
	image = (RETSIZE+3)*2
	dotlen= (RETSIZE+2)*2
	atr   = (RETSIZE+1)*2

	push	SI
	push	DI

	mov	AX,[BP+y]
	mov	BX,AX
	and	BX,3
	xor	AX,BX
	js	short OUTRANGE	; y���}�C�i�X�Ȃ�͈͊O
	mov	CX,AX
	shr	AX,1
	shr	AX,1
	add	AX,CX
	shl	AX,1
	add	AX,TextVramSeg
	mov	ES,AX

	mov	AX,[BP+x]
	cwd
	and	DX,AX
	sub	AX,DX
	mov	DI,AX		; DI = x, x���}�C�i�X�Ȃ� DI = 0, DX = x
	mov	CX,DX
	neg	CX
	mov	SI,[BP+image]	; SI = image
	and	CL,15
	rol	SI,CL		; ���ɂ͂ݏo�Ă���ꍇ�A���̂Ԃ�i�߂�
	not	SI
	mov	CX,BX		; CX = (y % 4)

	mov	BX,[BP+atr]	; BX = ����
	or	BL,10h		; cemi
	mov	BP,[BP+dotlen]	; BP = dotlen
	mov	AX,160		; �E�ɂ݂͂ł�?
	sub	AX,BP
	sub	AX,DI
	add	BP,DX
	cwd
	and	AX,DX
	add	BP,AX		; BP = dot length
	jle	short	OUTRANGE	; 1dot���łĂȂ��Ȃ�͈͊O


	shr	DI,1		; �ŏ��̓_�̃A�h���X�v�Z -> DI
	sbb	CH,CH		; �h�b�g�ʒu -> CL
	and	CH,4
	or	CL,CH
	shl	DI,1

	mov	AX,0101h
	shl	AX,CL
	mov	CL,4
	rol	AH,CL
	mov	CX,AX		; CL = bit mask, CH = next bit mask

	; �������炪�`�悾
XLOOP:
	mov	AX,BX
	xchg	ES:[DI+2000h],AX
	test	AL,10h
	mov	AX,0
	jz	short XCONT
	mov	AX,ES:[DI]
XCONT:
	or	AL,CL		; �����Ă�����
	rol	SI,1		; �_�𔲂����ǂ���
	sbb	DL,DL
	and	DL,CL
	xor	AL,DL		; ����
	dec	BP
	jle	short LASTWORD

	xchg	CL,CH			; ���̃h�b�g��
	cmp	CL,CH
	ja	short XCONT
	stosw
	jmp	short XLOOP
	EVEN
LASTWORD:
	stosw
;OUTRANGE:
	MRETURN
endfunc

END
