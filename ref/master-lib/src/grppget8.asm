; master library - graphic - packedpixel - get - hline - 8dot - PC98V
;
; Description:
;	�p�b�N�g4bit�s�N�Z���`���̐����O���t�B�b�N�p�^�[���ǂݎ��
;
; Function/Procedures:
;	void graph_pack_get_8( int x, int y, void far * linepat, int len ) ;
;
; Parameters:
;	x	�`��J�n x ���W( 0�`639, ������8�h�b�g�P�ʂɐ؂�̂� )
;	y	�`�� y ���W( 0 �` 399(��400���C���\���̂Ƃ�)
;	linepat	�p�^�[���f�[�^�̓ǂݎ���̐擪�A�h���X
;	len	�ǂݎ�鉡�h�b�g��
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
;	93/ 6/28 Initial: grppget8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] �N���b�v�g�ŉ��s���O�Ɣ��肵�Ă���(^^; fix
;	94/ 1/24 [M0.22] far�Œ�

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

func GRAPH_PACK_GET_8	; graph_pack_get_8() {
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

	mov	DI,[BP+linepat]

	mov	SI,[BP+x]
	sar	SI,3		; x��8�h�b�g�P�ʂɕ␳
	jns	short XNORMAL
	add	CX,SI
	jle	short CLIPOUT
	shl	SI,2
	add	DI,SI
	xor	SI,SI
XNORMAL:
	cmp	SI,SCREEN_XBYTE
	jge	short CLIPOUT

	add	CX,SI
	cmp	CX,SCREEN_XBYTE
	jl	short RIGHTCLIPPED
	mov	CX,SCREEN_XBYTE
RIGHTCLIPPED:
	sub	CX,SI
	imul	AX,AX,SCREEN_XBYTE
	add	SI,AX		; SI = draw address

	; �`��J�n
	push	DS
 	mov	ES,[BP+linepat+2]

	mov	DS,ClipYT_seg

	mov	BP,CX
XLOOP:
	mov	AL,[SI]		; 0a800h
	mov	BX,DS
	mov	AH,[SI+8000h]	; 0b000h
	add	BX,1000h
	mov	DS,BX
	mov	DL,[SI]		; 0b800h
	add	BX,2800h
	mov	DS,BX
	mov	DH,[SI]		; 0e000h
	sub	BX,3800h
	mov	DS,BX
	inc	SI

	push	SI

	mov	SI,4
LOOP8:
	mov	BL,BH
	mov	BH,CL
	mov	CL,CH

	rol	DH,1
	RCL	CH,1
	rol	DL,1
	RCL	CH,1
	rol	AH,1
	RCL	CH,1
	rol	AL,1
	RCL	CH,1

	rol	DH,1
	RCL	CH,1
	rol	DL,1
	RCL	CH,1
	rol	AH,1
	RCL	CH,1
	rol	AL,1
	RCL	CH,1

	dec	SI
	jnz	short LOOP8

	pop	SI

	mov	AX,BX
	stosw
	mov	AX,CX
	stosw

	dec	BP
	jnz	short XLOOP

	pop	DS

	MRETURN
endfunc		; }

END
