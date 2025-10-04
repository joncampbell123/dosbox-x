; master library - graphic - packedpixel - put - hline - 8dot - PC98V
;
; Description:
;	�h�b�g���Ƃ�1�o�C�g�̐����O���t�B�b�N�p�^�[���\��
;
; Function/Procedures:
;	void graph_unpack_put_8( int x, int y, void far * linepat, int len ) ;
;
; Parameters:
;	x	�`��J�n x ���W( 0�`639, ������8�h�b�g�P�ʂɐ؂�̂� )
;	y	�`�� y ���W( 0 �` 399(��400���C���\���̂Ƃ�)
;	linepat	�p�^�[���f�[�^�̐擪�A�h���X
;	len	�p�^�[���f�[�^�̉��h�b�g��
;		���������ۂɂ�8dot�P�ʂɐ؂�l�߂܂��B
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	�N���b�s���O�͏c�������� gc_poly �����̏������s���Ă��܂��B
;	���͉�ʕ��ŃN���b�s���O�ł��܂��B
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
; Revision History:
;	93/ 6/30 Initial: grpuput8.asm/master.lib 0.20
;	93/11/ 5 far�Œ�ɕύX

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XWIDTH equ 640
	SCREEN_XBYTE equ 80

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
endm

retfunc CLIPOUT
	MRETURN
endfunc

func GRAPH_UNPACK_PUT_8	; graph_unpack_put_8() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	linepat	= (RETSIZE+2)*2
	len	= (RETSIZE+1)*2

	mov	AX,[BP+y]
	sub	AX,ClipYT
	cmp	AX,ClipYH
	ja	short CLIPOUT

	mov	CX,[BP+len]
	sar	CX,3		; 8dot�P�ʂɐ؂�̂Ă�
	jle	short CLIPOUT

	mov	SI,[BP+linepat]

	mov	DI,[BP+x]
	cmp	DI,SCREEN_XWIDTH
	sar	DI,3		; x��8�h�b�g�P�ʂɕ␳
	jns	short XNORMAL
	add	CX,DI
	jle	short CLIPOUT
	shl	DI,2
	add	SI,DI
	xor	DI,DI
XNORMAL:
	cmp	DI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,DI
	imul	AX,AX,SCREEN_XBYTE
	add	DI,AX		; DI = draw address

	; �`��J�n
	push	DS
	mov	ES,ClipYT_seg
	mov	DS,[BP+linepat+2]
	CLD

	mov	BP,CX
XLOOP:
	mov	CX,4
	EVEN
LOOP8:
	lodsw
	shr	AL,1
	rcl	BL,1
	shr	AL,1
	rcl	BH,1
	shr	AL,1
	rcl	DL,1
	shr	AL,1
	rcl	DH,1

	shr	AH,1
	rcl	BL,1
	shr	AH,1
	rcl	BH,1
	shr	AH,1
	rcl	DL,1
	shr	AH,1
	rcl	DH,1
	loop	short LOOP8

	mov	ES:[DI],BL		; 0a800h
	mov	AX,ES
	mov	ES:[DI+8000h],BH	; 0b000h
	add	AX,1000h
	mov	ES,AX
	mov	ES:[DI],DL		; 0b800h
	add	AX,2800h
	mov	ES,AX
	mov	ES:[DI],DH		; 0e000h
	sub	AX,3800h
	mov	ES,AX
	inc	DI

	dec	BP
	jnz	short XLOOP

	pop	DS

	MRETURN
endfunc		; }

END
