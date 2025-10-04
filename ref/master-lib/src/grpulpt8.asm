; master library - graphic - large - put - hline - 8dot - PC98V
;
; Description:
;	�h�b�g���Ƃ�1�o�C�g�̐����O���t�B�b�N�p�^�[���\��(8dot�P��, 2x2�{)
;
; Function/Procedures:
;	void graph_unpack_large_put_8( int x, int y, void far * linepat, int len ) ;
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
;	���͉�ʕ��ŃN���b�s���O���Ă��܂��B
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
;	93/ 6/23 Initial: grppput8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] large model�ł�bugfix
;	93/ 7/ 3 [M0.20] �N���b�v�g�ŉ��s���O�Ɣ��肵�Ă���(^^; fix
;	93/ 7/28 [M0.20] ������Ɖ���
;	93/11/22 [M0.21] linepat��far�Œ艻
;	93/12/ 8 [M0.22] RotTbl�� rottbl.asm�ɕ���
;	94/ 2/15 Initial: grpulpt8.asm/master.lib 0.22a

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	SCREEN_XBYTE equ 80

	.CODE

RotTbl2	dd	00000000h,00000003h,00000300h,00000303h
	dd	00030000h,00030003h,00030300h,00030303h
	dd	03000000h,03000003h,03000300h,03000303h
	dd	03030000h,03030003h,03030300h,03030303h

buf_a	dw	0
buf_b	dw	0

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

func GRAPH_UNPACK_LARGE_PUT_8	; graph_unpack_large_put_8() {
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
	shl	CX,1		; 2�{

	mov	SI,[BP+linepat]

	mov	DI,[BP+x]
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
	shr	CX,1		; �f�[�^���̒����ɖ߂�
	imul	AX,AX,SCREEN_XBYTE
	add	DI,AX		; DI = draw address

	; �`��J�n
	push	DS
	mov	ES,ClipYT_seg
	mov	DS,[BP+linepat+2]

	mov	BP,CX
	CLD
	EVEN
XLOOP:
	mov	CL,2

	mov	BL,[SI]
	mov	BH,0
	shl	BL,CL
	mov	AX,word ptr CS:RotTbl2[BX]
	mov	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	shl	BL,CL
	or	AX,word ptr CS:RotTbl2[BX]
	or	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	ENDM
	mov	CS:buf_a,AX
	mov	CS:buf_b,DX

	mov	BL,[SI]
	shl	BL,CL
	mov	AX,word ptr CS:RotTbl2[BX]
	mov	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	shl	BL,CL
	or	AX,word ptr CS:RotTbl2[BX]
	or	DX,word ptr CS:RotTbl2[BX+2]
	inc	SI
	ENDM

	mov	CX,CS:buf_a
	xchg	CH,AL
	mov	ES:[DI],CX		; 0a800h
	mov	ES:[DI+SCREEN_XBYTE],CX
	mov	BX,ES
	mov	ES:[DI+8000h],AX	; 0b000h
	mov	ES:[DI+8000h+SCREEN_XBYTE],AX
	add	BH,10h

	mov	CX,CS:buf_b
	xchg	CH,DL
	mov	ES,BX
	mov	ES:[DI],CX		; 0b800h
	mov	ES:[DI+SCREEN_XBYTE],CX
	add	BH,28h
	mov	ES,BX
	mov	ES:[DI],DX		; 0e000h
	mov	ES:[DI+SCREEN_XBYTE],DX
	sub	BH,38h
	mov	ES,BX
	add	DI,2

	dec	BP
	ja	XLOOP

	pop	DS

	MRETURN
endfunc		; }

END
