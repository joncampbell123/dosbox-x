; master library - graphic - packedpixel - put - hline - 8dot - VGA - 16color
;
; Description:
;	VGA 16�F, �p�b�N�g4bit�s�N�Z���`���̐����O���t�B�b�N�p�^�[���\��
;
; Function/Procedures:
;	void vga4_pack_put_8( int x, int y, void far * linepat, int len ) ;
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
;	VGA
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�N���b�s���O�͏c�������� grc_setclip �ɑΉ����Ă��܂��B
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
;	93/ 6/23 Initial: grppput8.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] large model�ł�bugfix
;	93/ 7/ 3 [M0.20] �N���b�v�g�ŉ��s���O�Ɣ��肵�Ă���(^^; fix
;	93/ 7/28 [M0.20] ������Ɖ���
;	93/11/22 [M0.21] linepat��far�Œ艻
;	93/12/ 4 Initial: vg4pput8.asm/master.lib 0.22

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

USE_TABLE = 1

ROTATE	macro	wreg
	rept	2
	add	wreg,wreg
	ADC	CH,CH
	add	wreg,wreg
	ADC	CL,CL
	add	wreg,wreg
	ADC	BH,BH
	add	wreg,wreg
	ADC	BL,BL
	endm
endm

	.CODE

if USE_TABLE
	EXTRN RotTbl:DWORD
endif

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

func VGA4_PACK_PUT_8	; vga4_pack_put_8() {
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
	cmp	AX,ClipYT
	jl	short CLIPOUT
	cmp	AX,ClipYB
	jg	short CLIPOUT

	mov	CX,[BP+len]
	sar	CX,3		; 8dot�P�ʂɐ؂�̂Ă�
	jle	short CLIPOUT

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
	cmp	DI,graph_VramWidth
	jge	short CLIPOUT

	add	CX,DI
	cmp	CX,graph_VramWidth
	jl	short RIGHTCLIPPED
	mov	CX,graph_VramWidth
RIGHTCLIPPED:
	sub	CX,DI
	mul	graph_VramWidth
	add	DI,AX		; DI = draw address

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)	; mode0
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)	; pset
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)	; write directly
	out	DX,AX

	; �`��J�n
	push	DS
	mov	ES,graph_VramSeg
	mov	DS,[BP+linepat+2]
	CLD

	mov	BP,CX
	mov	DX,SEQ_PORT

	EVEN
XLOOP:
IF USE_TABLE
	mov	CL,2

	mov	BL,[SI]
	mov	BH,0
	shl	BX,CL
	mov	AX,word ptr CS:RotTbl[BX]
	mov	DX,word ptr CS:RotTbl[BX+2]
	inc	SI
	REPT 3
	shl	AX,CL
	shl	DX,CL
	mov	BL,[SI]
	mov	BH,0
	shl	BX,CL
	or	AX,word ptr CS:RotTbl[BX]
	or	DX,word ptr CS:RotTbl[BX+2]
	inc	SI
	ENDM
	mov	BX,AX
	mov	CX,DX
	mov	DX,SEQ_PORT
ELSE
	lodsw
	ROTATE	AL
	ROTATE	AH
	lodsw
	ROTATE	AL
	ROTATE	AH
ENDIF

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	out	DX,AX
	mov	ES:[DI],BL	; b

	mov	AH,2
	out	DX,AX
	mov	ES:[DI],BH	; r

	mov	AH,4
	out	DX,AX
	mov	ES:[DI],CL	; g

	mov	AH,8
	out	DX,AX
	mov	ES:[DI],CH	; i
	inc	DI

	dec	BP
IF USE_TABLE
	jnz	short XLOOP
ELSE
	jz	short owari
	jmp	XLOOP
owari:
ENDIF
;	mov	AH,0fh
;	out	DX,AX

	pop	DS

	MRETURN
endfunc		; }

END
