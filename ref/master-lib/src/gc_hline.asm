PAGE 98,120
; graphics - grcg - hline - PC98V
;
; DESCRIPTION:
;	�������̕`��(�v���[���̂�)
;
; FUNCTION:
;	void far _pascal grcg_hline( int xl, int xr, int y ) ;

; PARAMETERS:
;	int xl	���[��x���W
;	int xr	�E�[��x���W
;	int y	y���W
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; HISTORY:
;	92/6/6	Initial
;	92/6/16 TASM�Ή�
;	93/ 5/29 [M0.18] .CONST->.DATA

	.186

	.MODEL SMALL

	.DATA

	EXTRN	EDGES:WORD

	.DATA?

	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

; void far _pascal grcg_hline( int xl, int xr, int y ) ;
;
func GRCG_HLINE
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	xl = (RETSIZE+3)*2
	xr = (RETSIZE+2)*2
	 y = (RETSIZE+1)*2

	mov	DX,[BP+y]
	sub	DX,ClipYT	;
	cmp	DX,ClipYH	;
	ja	short RETURN	; y ���N���b�v�͈͊O -> skip

	mov	CX,[BP+xl]
	mov	BX,[BP+xr]

	mov	BP,DX		; BP=VRAM ADDR
	shl	BP,2		;
	add	BP,DX		;
	shl	BP,4		;

	mov	AX,ClipXL	; �N���b�v�g���[�����炷
	sub	CX,AX
	sub	BX,AX

	test	CX,BX		; x�����Ƀ}�C�i�X�Ȃ�͈͊O
	js	short RETURN

	cmp	CX,BX
	jg	short SKIP
	xchg	CX,BX		; CX�͕K���񕉂ɂȂ�
SKIP:
	cmp	BX,8000h	; if ( BX < 0 ) BX = 0 ;
	sbb	DX,DX
	and	BX,DX

	mov	DI,ClipXW
	sub	CX,DI		; if ( CX >= ClipXW ) CX = ClipXW ;
	sbb	DX,DX
	and	CX,DX
	add	CX,DI

	sub	CX,BX			; CX = bitlen
					; BX = left-x
	jl	short RETURN	; �Ƃ��ɉE�ɔ͈͊O �� skip

	mov	ES,ClipYT_seg	; �Z�O�����g�ݒ�

	add	BX,AX		; (AX=ClipXL)
	mov	DI,BX		; addr = yaddr + xl / 16 * 2 ;
	shr	DI,4
	shl	DI,1
	add	DI,BP

	and	BX,0Fh		; BX = xl & 15
	add	CX,BX
	sub	CX,16
	shl	BX,1
	mov	AX,[EDGES+BX]	; ���G�b�W
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short LAST
	stosw
	mov	AX,0FFFFh
	rep stosw
LAST:	and	AX,[EDGES+2+BX]	; �E�G�b�W
	stosw
RETURN:
	MRETURN
endfunc
END
